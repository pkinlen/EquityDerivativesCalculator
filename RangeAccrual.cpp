#include "RangeAccrual.hpp"
#include "MarketData.hpp"

RangeAccrualContract::RangeAccrualContract(const boost::property_tree::ptree &parentTree)
    : Contract( range_accrual, pt_get<std::string>(parentTree, "contract_id"))
{
	boost::property_tree::ptree pTree = parentTree.get_child("range_accrual");
    
	m_underlyingType     = pt_get<std::string> (pTree, "underlying_type");
	m_accCcy             = pt_get<std::string> (pTree, "accounting_ccy"); // i.e. payout currency
	m_undlCcy            = pt_get<std::string> (pTree, "underlying_ccy"); 
    m_firstAccrualDate   = pt_getDate          (pTree, "start_date");
	m_maturity           = pt_getDate          (pTree, "maturity_date");
	m_monthsPerPeriod    = pt_get<Size>        (pTree, "months_per_period");
    m_couponInitial      = pt_get<Real>        (pTree, "coupon_initial");
	m_couponStep         = pt_get<Real>        (pTree, "coupon_step");
    m_lowBarrierInitial  = pt_get<Real>        (pTree, "low_barrier_initial");
    m_lowBarrierStep     = pt_get<Real>        (pTree, "low_barrier_step");
	m_notional           = pt_get<Real>        (pTree, "notional");
	m_settlementLag      = pt_get<Size>        (pTree, "settlement_lag");

	m_redemptionPerUnitNotional = 1.0; // for now this is hard-coded

	pt_getVector<std::string>(pTree, "holiday_calendars", "id",    // inputs
                              m_holCalIDs);                        // outputs
}

class RangeAccrualMCEngine : public PathPricer<Path>
{
public:
	RangeAccrualContract*        m_RA_terms; // the range accrual contract (terms and conditions)
	MarketCaches*                m_marketCaches;

	boost::shared_ptr<Calendar>  m_obsHolCal;      // the holiday calendar for observation dates
	std::vector<Date>            m_periodEndDates;
	Size                         m_numPeriods;
	Size                         m_obsAlreadyInRangeThisPeriod;
	std::vector<Real>            m_numObsDates;           // aka N, ( length m_numPeriods ) 
	std::vector<Real>            m_barrierInEachPeriod;

	std::vector<Size>            m_periodIndex; // converts path index to period index, length: num of path elms
   	std::vector<Real>            m_discFactors;           // length m_numPeriods
	std::vector<Date>            m_periodSettleDates;     // length m_numPeriods
	std::vector<Real>            m_barriers;
	Size                         m_currentPeriod;
  
	Size                         m_pathLength;
	Real                         m_currentSpot;
	boost::shared_ptr<Prices>    m_spotPrices; // includes historics

	// We do some pre-calculation so that it doesn't have to be repeated for each path. 
	// The variables is the coupon rate times the day count fraction times the discount factor 
	// divided by the number of observations in the period.
    std::vector<Real>            m_CPNxDCFxDFoverNumObs;  // length m_numPeriods
    Real                         m_PVOfRedemption;
    ////////////////////////////////////////////////////////////////////////////////////////////////
	// we now have some private members that are used in initialization, called by the constructor
private:
	Real getDayCountFraction(Size period);

	void setCurrentSpotAndCurrencies();
	void setDiscFactors();
	void setPeriodDates();
	Size getObsAlreadyInRange() const; // uses historical spot prices
    Date getPeriodStart(Size period) const;
    void setPeriodIndex();
	RangeAccrualMCEngine() { QL_FAIL("RangeAccrualMCEngine(): Please don't use this constructor"); }


public:
	RangeAccrualMCEngine(RangeAccrualContract* ra_terms, MarketCaches* marketCaches);

    // returns the present value for given path.
	Real operator()(const Path& path) const;

	Size getNumMCTimeSteps();
};

Size RangeAccrualMCEngine::getNumMCTimeSteps()
{   // PK TODO: for a trade that will start in the future could do something like
	// startDate = std::min( firstAccrualDate, evalDate)
	return m_obsHolCal->businessDaysBetween(m_marketCaches->getEvalDate(), 
	                                        m_periodEndDates[m_numPeriods-1], 
											false, true);
}

Real RangeAccrualMCEngine::getDayCountFraction(Size period)
{   // could perhaps take as input the day count fraction convention that is used.
	return ((Real)m_RA_terms->m_monthsPerPeriod / 12.0);
}

void RangeAccrualMCEngine::setPeriodDates()
{
    Schedule schedule(m_RA_terms->m_firstAccrualDate, m_RA_terms->m_maturity,
                      Period(m_RA_terms->m_monthsPerPeriod, Months), *m_obsHolCal, 
					  Following, Following, DateGeneration::Backward, false);

	// we exclude the first element
	m_periodEndDates = subset<Date>(schedule.dates(), 1, schedule.dates().size()-1);    
	m_numPeriods     = m_periodEndDates.size(); 

	SettleRule sr(m_obsHolCal, m_obsHolCal, m_RA_terms->m_settlementLag, Following);
	m_periodSettleDates.resize(m_numPeriods);
	for(Size period = 0; period < m_numPeriods; period++)
		m_periodSettleDates[period] = sr.getSettleDate(m_periodEndDates[period]);


	writeDiagnostics("Have period end dates:\n" + toString(m_periodEndDates, "\n"));	
}

void RangeAccrualMCEngine::setPeriodIndex()
{
	m_periodIndex.resize(getNumMCTimeSteps()+1); // add on to include the evalDate 
	Date date = m_marketCaches->getEvalDate();
	Size period = m_currentPeriod; 
	for(Size pathIndex = 0; pathIndex < m_periodIndex.size(); pathIndex++)
	{ 
		m_periodIndex[pathIndex] = period;
		date = m_obsHolCal->advance(date, 1, Days, Following);
		if( date > m_periodEndDates[period])
			period++;
	}
	QL_REQUIRE(period == m_numPeriods, 
		       "RangeAccrualMCEngine::setPeriodDates(): Ended up in period "
			   << period << "\nbut the number of periods is:" << m_numPeriods);
	date = m_obsHolCal->advance(date, -1, Days, Preceding);
	
	if(date != m_periodEndDates[m_numPeriods-1])
		writeDiagnostics("Warning: expected date: " + toString(date) 
		                 + "\nto be equal to the final period end date " + toString(m_periodEndDates[m_numPeriods-1]),
						 low, "RangeAccrualMCEngine::setPeriodDates");
}

RangeAccrualMCEngine::RangeAccrualMCEngine(RangeAccrualContract*  ra_terms,
										   MarketCaches*          marketCaches)
{
	m_RA_terms     = ra_terms;
	m_marketCaches = marketCaches;
	setCurrentSpotAndCurrencies();
	m_obsHolCal = getHolCal(m_RA_terms->m_holCalIDs,            m_marketCaches, 
		                    m_RA_terms->m_firstAccrualDate, 
		                    m_RA_terms->m_maturity + 100 );
	setPeriodDates();

	m_barriers.resize(m_numPeriods);
	m_numObsDates.resize(m_numPeriods);
	m_CPNxDCFxDFoverNumObs.resize(m_numPeriods);
	setDiscFactors();
	bool foundCurrentPeriod = false;
	
	for( Size period = 0; period < m_numPeriods; period++)
	{   
		Date periodStart   = getPeriodStart(period);
		m_barriers[period] = m_RA_terms->m_lowBarrierInitial + m_RA_terms->m_lowBarrierStep * (Real) period;
		Real coupon        = m_RA_terms->m_couponInitial     + m_RA_terms->m_couponStep     * (Real) period;

		m_numObsDates[period] = m_obsHolCal->businessDaysBetween(periodStart, m_periodEndDates[period],
			                                                          true, true);
		QL_REQUIRE(m_numObsDates[period] > 0, 
		           "Found zero observation dates in period " << period  << "\nwith period start: " 
				   << periodStart << " and period end " << m_periodEndDates[period]);

		m_CPNxDCFxDFoverNumObs[period] = coupon * getDayCountFraction(period) * m_discFactors[period] 
	                                     / ((Real) m_numObsDates[period]);
		if(!foundCurrentPeriod && ( m_marketCaches->getEvalDate() <= m_periodEndDates[period]))
		{
			foundCurrentPeriod = true;
			m_currentPeriod    = period;
		}
	}
	setPeriodIndex();

	QL_REQUIRE(foundCurrentPeriod, "RangeAccrualMCEngine::RangeAccrualMCEngine:"
		       << "\nIt looks like the trade has already expired."
			   << "\nThe eval date is: " << m_marketCaches->getEvalDate() 
			   << ".\nand the last period end date is: " << m_periodEndDates[m_numPeriods-1]);

	m_obsAlreadyInRangeThisPeriod = getObsAlreadyInRange(); 
	m_PVOfRedemption              = m_RA_terms->m_redemptionPerUnitNotional * m_discFactors[m_numPeriods-1];
	writeDiagnostics("Found PV of final redemption to be: " + toString(m_PVOfRedemption), 
		             mid, "RangeAccrualMCEngine");
}

Date RangeAccrualMCEngine::getPeriodStart(Size period) const
{	
	if(period == 0)
		return m_RA_terms->m_firstAccrualDate;
	else // period > 0
		return m_obsHolCal->advance(m_periodEndDates[period - 1], +1, Days, Preceding);
}

Size RangeAccrualMCEngine::getObsAlreadyInRange() const // uses historical spot prices
{
	Size obsInRange = 0;
	for(Date d = getPeriodStart(m_currentPeriod); 
		     d < m_marketCaches->getEvalDate(); 
			 d = m_obsHolCal->advance(d, 1, Days, Following))
	{
		if( m_spotPrices->getPrice(d) > m_barriers[m_currentPeriod])
			obsInRange++; 
	}
	return obsInRange;
}

void RangeAccrualMCEngine::setDiscFactors()
{
	m_discFactors.assign(m_numPeriods, 1.0);
    MarketCaches::YieldTSCacheSmartPointer yieldTSCache = m_marketCaches->getYieldTSCache();
	Handle<YieldTermStructure> yieldTS (yieldTSCache->get( CONST_STR_risk_free_rate, m_RA_terms->m_accCcy));
    for(Size period = 0;  period < m_numPeriods; period++)
	{
		if(m_periodSettleDates[period] > m_marketCaches->getEvalDate())
			m_discFactors[period] = yieldTS->discount( m_periodSettleDates[period]);
	}
}

// calculate the actual value of the trade given one path
Real RangeAccrualMCEngine::operator ()(const Path& path) const
{
	std::vector<Size> numDaysInRange(m_numPeriods, 0);
    numDaysInRange[m_currentPeriod] = m_obsAlreadyInRangeThisPeriod;
	for(Size pathIndex = 0; pathIndex < path.length(); pathIndex++)
	{
		numDaysInRange[m_periodIndex[pathIndex] ] += (path[pathIndex] >= m_barriers[m_periodIndex[pathIndex] ] ? 1 : 0);
	}

	Real pv = m_PVOfRedemption;

	for(Size period = m_currentPeriod; period < m_numPeriods; period++)
		pv += (Real) numDaysInRange[period] * m_CPNxDCFxDFoverNumObs[period];

	return pv;
}

void RangeAccrualMCEngine::setCurrentSpotAndCurrencies()
{
	if( !strcmp(m_RA_terms->m_underlyingType.c_str(), "fx"))
	{		
		m_spotPrices   = m_marketCaches->getFXPricesCache()->get(m_RA_terms->m_undlCcy, m_RA_terms->m_accCcy);
		m_currentSpot  = m_spotPrices->getCurrentPrice();
		writeDiagnostics("\nWith accounting currency: " + m_RA_terms->m_undlCcy + " and underlying currency: "
			             + m_RA_terms->m_accCcy + "\nhave spot rate of " + toString(m_currentSpot),
						 high, "RangeAccrualMCEngine::setCurrentSpotAndCurrencies");
	}
	else
		QL_FAIL("RangeAccrualMCEngine::setCurrentSpot(): \ncurrently underlyingType must be 'fx', here it is: "
	           << m_RA_terms->m_underlyingType );
}

RangeAccrualCalculator::RangeAccrualCalculator(RangeAccrualContract*   pRA_terms, 
											   MarketCaches*           pMarketCaches,
		                                       ResultSet*              pResultSet)
    : CalculatorBase(pRA_terms, pMarketCaches, pResultSet)
{
	boost::shared_ptr<RangeAccrualMCEngine> RA_MCEngine = (boost::shared_ptr<RangeAccrualMCEngine>)
		  new RangeAccrualMCEngine(pRA_terms, pMarketCaches);

    Handle<Quote> underlyingH(boost::shared_ptr<Quote>(new SimpleQuote(RA_MCEngine->m_currentSpot)));
	Handle<YieldTermStructure> yieldTSAccCcy (pMarketCaches->getYieldTSCache()->
		                  get( CONST_STR_risk_free_rate, pRA_terms->m_accCcy));
	Handle<YieldTermStructure> yieldTSUndlCcy (pMarketCaches->getYieldTSCache()->
		                  get( CONST_STR_risk_free_rate, pRA_terms->m_undlCcy));

	Date finalAccrualDate = RA_MCEngine->m_periodEndDates[RA_MCEngine->m_numPeriods-1];

	Handle<BlackVolTermStructure> fxVolTS(pMarketCaches->getFXVolCache()
		                                  ->get(pRA_terms->m_accCcy, pRA_terms->m_undlCcy));

    boost::shared_ptr<StochasticProcess1D> stochasticPro(new BlackScholesMertonProcess
		(underlyingH, yieldTSUndlCcy, yieldTSAccCcy, fxVolTS));

    Size nTimeSteps = RA_MCEngine->getNumMCTimeSteps(); 
	PseudoRandom::rsg_type rsg = PseudoRandom::make_sequence_generator(
		                           nTimeSteps, 
				                   getConfig()->getRandomGeneratorSeed());

    bool brownianBridge = false;
    Time years = (finalAccrualDate - pMarketCaches->getEvalDate())/ 365.0;
    typedef SingleVariate<PseudoRandom>::path_generator_type generator_type;

	boost::shared_ptr<generator_type> myPathGenerator(new
        generator_type(stochasticPro, years, nTimeSteps, rsg, brownianBridge));

    // a statistics accumulator for the path-dependant Profit&Loss values
    Statistics statisticsAccumulator;
    bool antithetic = true;
    // The Monte Carlo model generates paths using myPathGenerator
    // each path is priced using myPathPricer
    // prices will be accumulated into statisticsAccumulator
    MonteCarloModel<SingleVariate,PseudoRandom> MCSimulation(myPathGenerator, RA_MCEngine,
                                                             statisticsAccumulator, antithetic );
    Size numSamples = atoi(getConfig()->get("range_accrual_num_mc_samples").c_str());
	writeDiagnostics("Number of MC samples being used is: " + toString(numSamples), 
	                 mid, "RangeAccrualCalculator");
    MCSimulation.addSamples(numSamples);
    
    Real pricePerUnitNotional = MCSimulation.sampleAccumulator().mean();
	Real errorEstimate = MCSimulation.sampleAccumulator().errorEstimate(); 
	writeDiagnostics("MC error estimate is " + toString(errorEstimate), mid, "RangeAccrualCalculator");
    
	/////////////////////////////////////////////////////////////////////////////
	// populate results
	boost::shared_ptr<Result> perUnitValRes = (boost::shared_ptr<Result>) new Result();
	perUnitValRes->setAttribute        ( contract_category,       "range_accrual",    true);
	perUnitValRes->setAttribute        ( contract_id,             pRA_terms->getID(), true);
	perUnitValRes->setAttribute        ( underlying_id,           pRA_terms->m_accCcy + pRA_terms->m_undlCcy, true);
	perUnitValRes->setValueAndCategory ( price_per_unit_notional, pricePerUnitNotional);
	pResultSet->addNewResult           ( perUnitValRes);

	boost::shared_ptr<Result> cashPriceRes = (boost::shared_ptr<Result>) new Result(*perUnitValRes);
	cashPriceRes->setValueAndCategory  ( cash_price, pricePerUnitNotional * pRA_terms->m_notional);
	cashPriceRes->setAttribute         ( currency,   pRA_terms->m_accCcy);
	pResultSet->addNewResult           ( cashPriceRes);
}
