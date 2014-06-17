#include "Accumulator.hpp"
#include "MarketData.hpp"
#include "Result.hpp"

template<class T> bool ascending (const T& a, const T& b) { return a <= b; }
template<class T> bool descending(const T& a, const T& b) { return a >= b; }

AccumulatorContract::AccumulatorContract(const boost::property_tree::ptree &pt)
		: Contract( koda, pt_get<std::string>(pt, "contract_id"))
{
	initialize(pt); 
}

void AccumulatorContract::initialize(const boost::property_tree::ptree &parentTree) 
{
	boost::property_tree::ptree pTree = parentTree.get_child("accumulator");

	m_firstAccumDate        = pt_getDate  (pTree, "first_accumulation_date"); 
	m_strikePrice           = pt_get<Real>(pTree, "strike_price"           );
	m_refSpot               = pt_get<Real>(pTree, "ref_spot"               );
    m_KOPrice               = pt_get<Real>(pTree, "ko_price"               );
	m_sharesPerDay          = pt_get<Real>(pTree, "shares_per_day"         );

    // Now to set some optional parameters.         tree   key                          default
	m_KOSettleIsAtPeriodEnd = pt_get_optional<bool>(pTree, "ko_settlement_at_period_end", true);
	m_positionSize          = pt_get_optional<Real>(pTree, "position_size",                1.0);
	m_gearingStrike         = pt_get_optional<Real>(pTree, "gearing_price",      m_strikePrice);
  	m_gearingMultiplier     = pt_get_optional<Real>(pTree, "gearing_multiplier" ,          1.0);

    m_hasGearing            = ( m_gearingStrike > 0) && (m_gearingMultiplier != 1.0 );
	m_maxGearingMultiplier  = std::max( 1.0, m_gearingMultiplier); // expected to be 1 (when no gearing) or 2 with gearing

	// To help the calc speed, we do a few multiplcations here. Faster to do so once here, rather than 
	// frequently in the main MC calculation body. 
    m_maxGearingTimesStrike = m_maxGearingMultiplier * m_strikePrice;  
	m_sharesPerDayTimesStrike = m_sharesPerDay * m_strikePrice; 

	m_underlyingID          = pt_get         <std::string>(pTree, "underlying_id");
	m_underlyingIDType      = pt_get_optional<std::string>(pTree, "underlying_id_type",             "");
	m_settleLag             = pt_get_optional<Integer>    (pTree, "settlement_lag",                  0);
	m_hasGuaranteedAccum    = pt_get_optional<bool>       (pTree, "has_guaranteed_accumulation", false);

	m_issueDate             = pt_getDateOptional(pTree, "issue_date",                 m_firstAccumDate);
	m_lastGuaranteedAccum   = pt_getDateOptional(pTree, "last_guaranteed_accum_date", m_firstAccumDate - 1);
    m_firstKODate           = pt_getDateOptional(pTree, "first_ko_date", 
		                                           std::min(m_issueDate + 1, m_firstAccumDate));

    getVectorOfDatesFromPTree(pTree, "period_end_dates", "date",  // inputs
		                      m_periodEndDates);                  // output

    std::sort(m_periodEndDates.begin(), m_periodEndDates.end(), ascending<Date>);
    m_numPeriods            = m_periodEndDates.size();

	std::string noteOrSwap  = pt_get_optional<std::string>(pTree, "note_or_swap",       "swap");
	std::string accumOrDecum= pt_get_optional<std::string>(pTree, "accum_or_decum",    "accum");

	QL_REQUIRE(!(strcmp(noteOrSwap.c_str(), "note") && strcmp(noteOrSwap.c_str(), "swap"))
		,"AccumulatorContract::AccumulatorContract(..): For trade: " 
		<< getID() << "'note_or_swap' must be 'note' or 'swap', here found it to be: "
		<< noteOrSwap);

	QL_REQUIRE(!(strcmp(accumOrDecum.c_str(), "accum") && strcmp(accumOrDecum.c_str(), "decum")),
		"AccumulatorContract::AccumulatorContract(..): For trade: " 
		<< getID() << "'accum_or_decum' must be 'accum' or 'decum', here found it to be: "
		<< accumOrDecum);

	QL_REQUIRE(strcmp(accumOrDecum.c_str(), "decum"),
		"AccumulatorContract::AccumulatorContract(..): For trade: " 
		<< getID() << " for now can't have 'accum_or_decum' set to 'decum'. ");

    QL_REQUIRE(!strcmp(noteOrSwap.c_str(), "swap"), 
		"AccumulatorContract::AccumulatorContract(..): For trade: " 
		<< getID() << " for now 'noteOrSwap' must be 'swap'. ");

	m_subCategory           = accumulator_swap;  

}

struct OptionInputs {
    Option::Type type;
    Real underlying;
    Real strike;
    Spread dividendYield;
    Rate riskFreeRate;
    Volatility volatility;
    Date maturity;
    DayCounter dayCounter;
};


struct AccumDeliveries
{
	std::vector<Real>    m_sharesDelivered; // size m_numPeriods
	std::vector<Real>    m_cashDelivered;   // size m_numPeriods

	void reset(const std::vector<Real>& shares, const std::vector<Real>& cash)
    {
       m_sharesDelivered = shares;
       m_cashDelivered   = cash;   
    }
};

class AccumulatorMCEngine : public PathPricer<Path>
{
private:
	std::vector<Real>             m_sharesDeliveredDueToHistoricalAccumulation;
	std::vector<Real>             m_cashDeliveredDueToHistoricalAccumulation;

    AccumulatorContract*          m_accumContract;
    MarketCaches*                 m_marketCaches;
	boost::shared_ptr<Prices>     m_prices;

    // m_indexOnOrBeforeEval is -1 when evalDate is before all accum dates
	// when evalDate is after all accum dates it is set to the index of the last
	// when evalDate is between two accum dates, it is set to the one immediately before.
	Integer                       m_indexOnOrBeforeEval; 
	Size                          m_currentPeriod;
	bool                          m_evalDateIsHoliday;
	std::vector<Real>             m_discFactors;

	boost::shared_ptr<StockData>  m_stockData;
	std::string                   m_currency;             
	Size                          m_totNumAccumDays;
	std::vector<Date>             m_accumDates;              // size m_totNumAccumDays

	// m_periodIndexOfDate will look like: { 0,...,0,1,...,1,2,...,2,...,numPeriods-1,...,numPeriods-1 } 
    std::vector<Size>             m_periodIndexOfDate;       // size m_totNumAccumDays
    std::vector<Date>             m_periodSettleDates;       // size m_numPeriods
	std::vector<Size>             m_indexOfPeriodEnd;        // size m_numPeriods 

	boost::shared_ptr<Calendar>   m_underlyingHolCal;
    boost::shared_ptr<Schedule>   m_busDaySchedule;

	void checkAccumContract(); // throws on failure
    void generateDates ();     // initialized the data information.

	AccumulatorMCEngine() {}; // don't want this constructor to be used.
public:
	AccumulatorMCEngine(AccumulatorContract* pContract, MarketCaches* pMarket); 

	void initialize();
    void setHistoricalAccum();
	void setDiscFactors();
	Size getNumMCTimeSteps(); 

    // returns the present value for given path.
	Real operator()(const Path& path) const;

    // oneDaysAccumulation(..) returns true if the KO is triggered, otherwise false.
    // It assumes there is no KO before this.
    // Will amend the cashDelivered and sharesDelivered output parameters adding the 
    // contibution of one day.
    bool oneDaysAccumulation(Real spot,                                   // input
						     Real& sharesDelivered, Real& cashDelivered)  // outputs 
                             const;   // const method doesn't change any member variables

    Size getPathIndexFromDateIndex(Size dateIndex) const;
    Size getDateIndexFromPathIndex(Size pathIndex) const;

    Real getPeriodEndSharePrice  (Size period,                     const Path& path)  const;
	Real getPresentValue         (bool knockedOut, Size indexOfKO, const Path& path)  const; // for a given path

	Real sumPeriodEndContibutions(bool knockedOut,  Size indexOfKO, 
		                          const Path& path, const AccumDeliveries& deliveries) const;

	Size                        getIndexOfPeriodStart (Size period)  const;
    Real                        getGearingMultiplier  (Real spot)    const;
   	boost::shared_ptr<Calendar> getUndlHolCal         ()             const;
	Real                        getRemainingNotional  ()             const;
	std::string                 getCurrency           ()             const;
};

boost::shared_ptr<Calendar> AccumulatorMCEngine::getUndlHolCal() const
{
	QL_REQUIRE(m_underlyingHolCal != NULL, 
		       "AccumulatorMCEngine::getUndlHolCal(.): The underlying holiday calendar has not yet been set.");

	return m_underlyingHolCal;
}

std::string AccumulatorMCEngine::getCurrency() const
{
	return m_currency;
}

Real AccumulatorMCEngine::getRemainingNotional() const
{
	// First we calculate the remaining days accumulation from the beginning of the current period.
	Real remainingDaysAccum = (Real)m_totNumAccumDays - getIndexOfPeriodStart(m_currentPeriod);
    Real remainingNotional  = remainingDaysAccum * m_accumContract->m_maxGearingTimesStrike 
		                                         * m_accumContract->m_sharesPerDay;

	QL_REQUIRE(remainingNotional > 0, "AccumulatorMCEngine::getRemainingNotional():\n"
         		<< "Remaining notional must be greater than zero, here it was found to be: "
				<< remainingNotional << "\nwith remaining the number of days accum =" << remainingDaysAccum 
				<< " and max gearing times strike " << m_accumContract->m_maxGearingTimesStrike);

	return remainingNotional;
}

AccumulatorMCEngine::AccumulatorMCEngine(AccumulatorContract* pContract, 
										 MarketCaches*        pMarket) 
{
	m_accumContract   = pContract;
	m_marketCaches    = pMarket;
   
	initialize();
}

Size AccumulatorMCEngine::getIndexOfPeriodStart(Size period) const
{   
	return (period == 0 ? 0 : m_indexOfPeriodEnd[period - 1] + 1);	
}

// If evalDate is after end of vDates,        then the index is set to the last element.
// If evalDate is between two vDates,         then the index is set to the one just before evalDate.
// If evalDate is before the first of vDates, then the index is set to -1 ( i.e. an invalid index)
// and because we use -1, we can't use the unsigned type 'Size', 
// so we use (signed) 'Integer' as the return type. 
Integer findIndexOfEvalDate(const Date& evalDate, const std::vector<Date> vDates) 
{   // If an efficiency improvement is required, we could use binary search or something in the stl.
	// We can assume that vDates are strictly increasing.
	// However this function is call very infrequently and so efficiency is not a big deal.
	Size i = 0;
	while( (i < vDates.size()) && (vDates[i] < evalDate))
	{  i++;    }

	// If we've gone too far need to go back 1, i.e. subtract 1
	return (Integer) i - (vDates[i] > evalDate ? 1 : 0);
}

void AccumulatorMCEngine::initialize() 
{
   m_stockData = m_marketCaches->getStockDataCache()->get(m_accumContract->m_underlyingID,
		                                                   m_accumContract->m_underlyingIDType);

   m_underlyingHolCal = m_marketCaches->getCalendarCache()->get(m_stockData->getHolidayCalendarID());

   m_currency = m_stockData->getCurrency();
   checkAccumContract(); // throws on failure, 

   generateDates();

   m_indexOnOrBeforeEval = findIndexOfEvalDate(m_marketCaches->getEvalDate(), m_accumDates);
   m_currentPeriod       = m_periodIndexOfDate[m_indexOnOrBeforeEval];
   setDiscFactors();

   m_prices = m_marketCaches->getStockPricesCache()->get(m_accumContract->m_underlyingID,
		                                            m_accumContract->m_underlyingIDType);

   // contributions from historical periods will be dropped.
   // but there is a historical contibution to the current period
   m_sharesDeliveredDueToHistoricalAccumulation.assign(m_accumContract->m_numPeriods, 0.0); // fill with zeros
   m_cashDeliveredDueToHistoricalAccumulation.assign  (m_accumContract->m_numPeriods, 0.0);   // fill with zeros

   setHistoricalAccum();
}

void AccumulatorMCEngine::checkAccumContract() // throws on failure
{
	QL_REQUIRE(    (m_accumContract->m_subCategory == accumulator_note) 
		        || (m_accumContract->m_subCategory == accumulator_swap),
				"AccumulatorMCEngine::checkAccumContract(): Currently can not deal with decumulators, \n"
				<< "Here AccumSubCategory is set to: " << m_accumContract->m_subCategory); 

	for(Size period = 0; period < m_accumContract->m_numPeriods; period++)
	{ 	
		if( !(getUndlHolCal()->isBusinessDay(m_accumContract->m_periodEndDates[period])))
		{   
			Date adjustedDate = getUndlHolCal()->adjust(m_accumContract->m_periodEndDates[period]);
		    writeDiagnostics(  "Warning:\nFor period " + toString(period + 1) + " found the end date to be: "
			                 + toString(m_accumContract->m_periodEndDates[period], "ddd d-mmm-yyyy")
						     + "\nwhich is a non-business day.\nSo moved the period end to: "
			                 + toString(adjustedDate, "ddd d-mmm-yyyy"),
							 low, "AccumulatorMCEngine::checkAccumContract");

			m_accumContract->m_periodEndDates[period] = adjustedDate;
		}
	}
}

// The following function could set zero business days in vBusDays, if startDate == endDate
// and that date is a non-business day
void setVectorOfBusinessDays(Date startDate, Date endDate, Calendar* cal, // inputs
				  		     std::vector<Date>& vBusDays)                 // outputs
{
	QL_REQUIRE(startDate <= endDate, "setVectorOfBusinessDays(..): startDate (" << startDate
		       << ") must be on or before end date (" << endDate << ").");
	
	vBusDays.clear();

	Date d = startDate;
	while( d <= endDate)
	{
		if( cal->isBusinessDay(d))
			vBusDays.push_back(d);

		d++;
	}
	writeDiagnostics(  "Generated a business day schedule with " + toString(vBusDays.size())
		             + " days.\nFirst: " + toString(startDate, "ddd d-mmm-yyyy") 
					 + ", last: " + toString(endDate, "ddd d-mmm-yyyy"),
					 mid, "setVectorOfBusinessDays");
}

void AccumulatorMCEngine::generateDates()
{
	setVectorOfBusinessDays(m_accumContract->m_firstAccumDate,  // input 
   		                    m_accumContract->m_periodEndDates[m_accumContract->m_numPeriods - 1],
							&(*m_underlyingHolCal),             // input
                            m_accumDates);                      // output

	m_totNumAccumDays = m_accumDates.size();

	std::string msg   =   "Generated accumDates with " + toString(m_totNumAccumDays) + " elements.\n"
		                + "First date is " + toString(m_accumDates[0]) 
		                + "\nand the last is " + toString(m_accumDates[m_totNumAccumDays-1]); 

	writeDiagnostics(msg, high, "AccumulatorMCEngine::generateDates");

	m_periodIndexOfDate.resize(m_totNumAccumDays);
	m_indexOfPeriodEnd.resize(m_accumContract->m_numPeriods);
	Size period = 0;
	for(Size day = 0; day < m_totNumAccumDays; day++)
	{
		period = std::min(period, m_accumContract->m_numPeriods - 1);
		// m_periodIndexOfDate will look like: { 0,...,0,1,...,1,2,...,2,...,numPeriods-1,...,numPeriods-1 } 
        m_periodIndexOfDate[day] = period;
		if( m_accumDates[day] >= m_accumContract->m_periodEndDates[period])
		{
		    if ( m_accumDates[day] > m_accumContract->m_periodEndDates[period])
			{   
				std::ostringstream stream;
			    stream << "Warning: for period: " << period + 1 // reporting the period using a counting base of 1
				       << "\nfound period end date to be: " << m_accumContract->m_periodEndDates[period]
			           << "\nwhich is not a business day. So we've moved that period end date to \n"
                       << m_accumDates[day];

			    writeDiagnostics(stream.str(), low, "AccumulatorMCEngine::generateDates");
     			m_accumContract->m_periodEndDates[period] = m_accumDates[day];
		     }
             m_indexOfPeriodEnd[period] = day;
			 period++;
		}
	}
	m_periodSettleDates.resize(m_accumContract->m_numPeriods);
	for(period = 0; period < m_accumContract->m_numPeriods; period++)
	{
		m_periodSettleDates[period] = m_underlyingHolCal->advance( m_accumContract->m_periodEndDates[period], 
		                                                           m_accumContract->m_settleLag, Days);
	}
}

Size AccumulatorMCEngine::getNumMCTimeSteps()
{ 
	return (Size)((Integer)m_totNumAccumDays - m_indexOnOrBeforeEval - 1); 
} 

void AccumulatorMCEngine::setDiscFactors()
{
	m_discFactors.assign(m_accumContract->m_numPeriods, 1.0);
    MarketCaches::YieldTSCacheSmartPointer yieldTSCache = m_marketCaches->getYieldTSCache();
	Handle<YieldTermStructure> yieldTS (yieldTSCache->get( CONST_STR_risk_free_rate, 
		                                                   m_currency));
    for(Size period = m_currentPeriod;  period < m_accumContract->m_numPeriods; period++)
	{
		QL_REQUIRE(m_periodSettleDates[period] >= m_marketCaches->getEvalDate(),
			       "AccumulatorMCEngine::setDiscFactors(): In period " << period
				   << "\nfound settle date: "         << toString(m_periodSettleDates[period])
				   << "\nwhich is before eval date: " << toString(m_marketCaches->getEvalDate())
				   << ".\nCurrent period is: "        << toString(m_currentPeriod));

		m_discFactors[period] = yieldTS->discount( m_periodSettleDates[period]);
	}
}

// The historical accumulation is within the current period, 
// i.e. after the last period end until yesterday. 
void AccumulatorMCEngine::setHistoricalAccum()
{
   Size indexOfCurrentPeriodStart = getIndexOfPeriodStart(m_currentPeriod);
   // when evalDate is before the first accum date m_indexOnOrBeforeEval will be set to -1

   Size period = m_currentPeriod;
   for(Size indexOfDate = indexOfCurrentPeriodStart; (Integer) indexOfDate <= m_indexOnOrBeforeEval; indexOfDate++)
   {   
	   if(oneDaysAccumulation(m_prices->getPrice(m_accumDates[indexOfDate]), 
		                      m_sharesDeliveredDueToHistoricalAccumulation[period], // will be amended
							  m_cashDeliveredDueToHistoricalAccumulation[period]))  // will be amended
	   {  // the trade has knocked out,  
          QL_REQUIRE( m_accumDates[indexOfDate] == m_marketCaches->getEvalDate(),
		              "AccumulatorMCEngine::initialize(): Found trade alread knocked out:"
		              << "\nDate:      " << m_accumDates[indexOfDate]        
				      << "\nSpot:      " << m_prices->getPrice(m_accumDates[indexOfDate]) 
				      << "\nKnock-out: " << m_accumContract->m_KOPrice                
				      << "\nStock-ID:  " << m_accumContract->m_underlyingID);
	   }
   } 
}

Size AccumulatorMCEngine::getPathIndexFromDateIndex(Size dateIndex) const
{   // we first cast the unsigned (Size) dateIndex to a signed Integer to deal
	// with the case when m_indexOnOrBeforeEval is -1
	QL_REQUIRE( (Integer)dateIndex >= m_indexOnOrBeforeEval,
 		        "AccumulatorMCEngine::getPathIndexFromDateIndex(dateIndex):\n"
				"Can't get path index from date index when dateIndex is " << dateIndex
                << "\nand index on or before eval is " << m_indexOnOrBeforeEval
				<< "\nas that would suggest a negative index.");

    return (Size)((Integer)dateIndex - m_indexOnOrBeforeEval); 
}	

Size AccumulatorMCEngine::getDateIndexFromPathIndex(Size pathIndex) const
{   // we first cast the unsigned (Size) pathIndex to a signed Integer to deal
	// with the case when m_indexOnOrBeforeEval is -1
	return  (Size)(m_indexOnOrBeforeEval + (Integer)pathIndex);
}

Real AccumulatorMCEngine::getPeriodEndSharePrice(Size period, const Path& path) const
{
    return path.value(getPathIndexFromDateIndex(m_indexOfPeriodEnd[period]));	
}

Real AccumulatorMCEngine::sumPeriodEndContibutions(bool knockedOut, Size indexOfKO, 
												   const Path& path, const AccumDeliveries& deliveries) const
{   // for now this method assumes that the KO settlements will be at period end.
	Real pv = 0;
	for(Size period = m_currentPeriod; period < m_accumContract->m_numPeriods; period++)
	{
	   pv += m_discFactors[period] * 
		     ( deliveries.m_sharesDelivered[period] * getPeriodEndSharePrice(period, path) 
		       + deliveries.m_cashDelivered[period] );
	}

	// When there's a KO, we need to return the remaining notional for note form accumulators.
	if(knockedOut && (m_accumContract->m_subCategory == accumulator_swap)) 
		pv += (m_totNumAccumDays - 1 - indexOfKO) 
		      * m_accumContract->m_maxGearingTimesStrike 
		      * m_discFactors[m_periodIndexOfDate[indexOfKO]];

	return pv;
}

// calculate the actual value of the trade given one path
Real AccumulatorMCEngine::operator ()(const Path& path) const
{
	AccumDeliveries deliveries;
	deliveries.reset(m_sharesDeliveredDueToHistoricalAccumulation, m_cashDeliveredDueToHistoricalAccumulation);

	bool knockedOut = false;
	Size indexOfKO  = m_totNumAccumDays; // Knocked out after end of trade, i.e. not knocked out.
	Size pathIndex  = 1;                 // Today's accumulation is deal with in the historical part. 
	while( (pathIndex < path.length()) && !knockedOut)
	{
		Size dateIndex = getDateIndexFromPathIndex(pathIndex);
		// oneDaysAccumulation(..) will return true if the trade has knocked out, otherwise false
		if(oneDaysAccumulation(path.value(pathIndex),                                          // input 
			                   deliveries.m_sharesDelivered[m_periodIndexOfDate[dateIndex] ],  // output
							   deliveries.m_cashDelivered  [m_periodIndexOfDate[dateIndex]]))  // output
		{
		    knockedOut = true;
            indexOfKO  = dateIndex;
		}
		pathIndex++;
	}
	return sumPeriodEndContibutions(knockedOut, indexOfKO, path, deliveries);
}

Real AccumulatorMCEngine::getGearingMultiplier(Real spot) const
{
	return (spot < m_accumContract->m_gearingStrike ? m_accumContract->m_gearingMultiplier : 1 ); 
}

// oneDaysAccumulation(..) returns true if the KO is triggered, otherwise false.
// It assumes there is no KO before this.
// Will amend the cashDelivered and sharesDelivered output parameters, adding the 
// contibution of one day.
bool AccumulatorMCEngine::oneDaysAccumulation(Real spot,                                  // input
								 			  Real& sharesDelivered, Real& cashDelivered) // outputs 
                                              const
{  // This method is going to be called for each step of each path in the accumulation, 
   // i.e. very very often.
   // So it may be worth spending some time to make it run efficently.

   Real gearingMultiplier = getGearingMultiplier(spot);
   sharesDelivered   += m_accumContract->m_sharesPerDay * gearingMultiplier; // setting the output variable
 
   if(m_accumContract->m_subCategory == accumulator_note) // can give back some notional to the holder, 
	                                 // positive amount: money from issuer to holder
      cashDelivered +=  ( m_accumContract->m_maxGearingMultiplier - gearingMultiplier ) 
	                   *  m_accumContract->m_sharesPerDayTimesStrike;
   else // is swap form, aka OTC, a negative amount indicates cash paid by holder to issuer.
	  cashDelivered += - gearingMultiplier * m_accumContract->m_sharesPerDayTimesStrike;
 
   return (spot >= m_accumContract->m_KOPrice);
}

// The constructor does the work to generate the results.
AccumulatorCalculator::AccumulatorCalculator(
	         AccumulatorContract* pAccumContract, MarketCaches* pMarketCaches,
		     ResultSet* pResultSet) // constructor will set the resultSet
				  : CalculatorBase(pAccumContract, pMarketCaches, pResultSet)
{
	boost::shared_ptr<AccumulatorMCEngine> accumMCEngine = (boost::shared_ptr<AccumulatorMCEngine>)
		 new AccumulatorMCEngine(pAccumContract, pMarketCaches);

	boost::shared_ptr<StockData> stockData = pMarketCaches->getStockDataCache()->
		                 get(pAccumContract->m_underlyingID, pAccumContract->m_underlyingIDType);

	std::string stockCurrency = stockData->getCurrency();

	boost::shared_ptr<Prices> prices = pMarketCaches->getStockPricesCache()->
		                 get(pAccumContract->m_underlyingID, pAccumContract->m_underlyingIDType);

    Handle<Quote> underlyingH(boost::shared_ptr<Quote>(new SimpleQuote(prices->getCurrentPrice())));

	Handle<YieldTermStructure> yieldTS (pMarketCaches->getYieldTSCache()->get( CONST_STR_risk_free_rate, stockCurrency));

    Handle<YieldTermStructure> dividendYieldTS(boost::shared_ptr<YieldTermStructure>(
         new FlatForward(pMarketCaches->getEvalDate(), stockData->getDividendYield(), Actual365Fixed())));

	Date finalAccumDate = pAccumContract->m_periodEndDates[pAccumContract->m_numPeriods-1];

	Handle<BlackVolTermStructure> flatVolTS(boost::shared_ptr<BlackVolTermStructure>(
            new BlackConstantVol(finalAccumDate, *(accumMCEngine->getUndlHolCal()) , stockData->getFlatVol(), Actual365Fixed())));

	//////////////////////////////////////////////////////////////////////
    boost::shared_ptr<StochasticProcess1D> stochasticPro(new BlackScholesMertonProcess
		(underlyingH, dividendYieldTS, yieldTS, flatVolTS));

    Size nTimeSteps = accumMCEngine->getNumMCTimeSteps();
	PseudoRandom::rsg_type rsg = PseudoRandom::make_sequence_generator(
		                           nTimeSteps, 
				                   getConfig()->getRandomGeneratorSeed());

    bool brownianBridge = false;
    Time years = (finalAccumDate - pMarketCaches->getEvalDate())/ 365.0;
    typedef SingleVariate<PseudoRandom>::path_generator_type generator_type;

	boost::shared_ptr<generator_type> myPathGenerator(new
        generator_type(stochasticPro, years, nTimeSteps, rsg, brownianBridge));

    // a statistics accumulator for the path-dependant Profit&Loss values
    Statistics statisticsAccumulator;
    bool antithetic = true;
    // The Monte Carlo model generates paths using myPathGenerator
    // each path is priced using myPathPricer
    // prices will be accumulated into statisticsAccumulator
    MonteCarloModel<SingleVariate,PseudoRandom> MCSimulation(myPathGenerator, accumMCEngine,
                                                             statisticsAccumulator, antithetic );
    
    MCSimulation.addSamples(atoi(getConfig()->get("accumulator_num_mc_samples").c_str()));
    
    Real cashValue     = MCSimulation.sampleAccumulator().mean();
	Real errorEstimate = MCSimulation.sampleAccumulator().errorEstimate() / accumMCEngine->getRemainingNotional(); 
	writeDiagnostics("Error estimate is " + toString(errorEstimate), mid, "Accum");
    
	/////////////////////////////////////////////////////////////////////////////
	// populate results
	boost::shared_ptr<Result> perUnitValRes = (boost::shared_ptr<Result>) new Result();
	Real pricePerUnitRemainingNotional = cashValue / accumMCEngine->getRemainingNotional();
	perUnitValRes->setValueAndCategory(price_per_unit_notional, pricePerUnitRemainingNotional);
	perUnitValRes->setAttribute(contract_category, "accumulator",                  true);
	perUnitValRes->setAttribute(contract_id,       pAccumContract->getID(),        true);
	perUnitValRes->setAttribute(underlying_id,     pAccumContract->m_underlyingID, true);
	pResultSet->addNewResult(perUnitValRes);

	// we don't have to reset the attributes because we are using the copy constructor
	boost::shared_ptr<Result> cashValRes = (boost::shared_ptr<Result>) new Result(*perUnitValRes);
	cashValRes->setAttribute(currency, accumMCEngine->getCurrency(), true);
	cashValRes->setValueAndCategory(cash_price, cashValue);
    pResultSet->addNewResult(cashValRes);
}

