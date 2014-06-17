#include "CallSpreadCpnNote.hpp"
#include "MarketData.hpp"
#include "Result.hpp"

CallSpreadCoupon::CallSpreadCoupon()
{
	m_initialized = false;
}

// In a call spread coupon note, the coupons are of the form:
//    coupon_paid = min( cap, max( floor, factor * underlying - strike))
// For a put-spread coupon, can set the 'factor' and 'strike' to be negative.

CallSpreadCoupon::CallSpreadCoupon(Real factor,                Real strike, 
								   boost::optional<Real> cap,  boost::optional<Real> floor)
{
	reset(factor, strike, cap, floor);
}

void CallSpreadCoupon::reset(Real factor,                Real strike, 
							 boost::optional<Real> cap,  boost::optional<Real> floor)
{
	m_factor = factor;
	m_strike = strike;
    if(m_capIsActive = cap)
		m_cap = cap.get();
    // else leave m_cap uninitialized

    if(m_floorIsActive = floor)
		m_floor = floor.get();
	// else leave m_floor uninitialized

	if(m_floorIsActive && m_capIsActive)
		QL_REQUIRE(m_floor <= m_cap,
         		   "CallSpreadCoupon::reset(..): don't allow the floor: " << m_floor
				   << "\nto be greater than the cap: " << m_cap);

	m_initialized = true;

	writeDiagnostics("Set call-spread coupon with: \ncap:    "
 		                            + (m_capIsActive   ? toString(m_cap)   : toString("not active"))
					 + "\nfloor:  " + (m_floorIsActive ? toString(m_floor) : toString("not active"))
					 + "\nfactor: " + toString(factor)
					 + "\nstrike: " + toString(strike),
		             high, "CallSpreadCoupon::reset");
}

Real CallSpreadCoupon::getCouponRate(Real spot) const // will throw if uninitialized
{   // the result will not yet be multiplied by the day count fraction.
	QL_REQUIRE(m_initialized, "CallSpreadCoupon::getCouponRate(.):"
		                      << "\nCan't get coupon rate when CallSpreadCoupon has not been initialized.");

    Real cpnRate = spot * m_factor - m_strike;
	if(m_floorIsActive)
		cpnRate = std::max( m_floor, cpnRate);
    
	if(m_capIsActive)
		cpnRate = std::min( m_cap, cpnRate);

	return cpnRate;
}

CallSpreadCpnNoteContract::CallSpreadCpnNoteContract(const boost::property_tree::ptree &parentTree)
    : Contract( call_spread_cpn_note, pt_get<std::string>(parentTree, "contract_id"))
{
	boost::property_tree::ptree pTree = parentTree.get_child("call_spread_cpn_note");
    
	m_startDate          = pt_getDate         (pTree, "start_date");
	m_finalObsDate       = pt_getDate         (pTree, "final_obs_date");
	m_rebate             = pt_get<Real>       (pTree, "rebate_per_unit_notional");
	m_monthsPerPeriod    = pt_get<Size>       (pTree, "months_per_period");
	m_underlyingType     = pt_get<std::string>(pTree, "underlying_type"); 
	m_accCcy             = pt_get<std::string>(pTree, "accounting_currency"); // aka payout currency
	m_undlCcy            = pt_get<std::string>(pTree, "underlying_currency");	
	m_settlementLag      = pt_get<Size>       (pTree, "settlement_lag");
	m_isCallable         = pt_get<bool>       (pTree, "is_callable");	
	m_issuerID           = pt_get<std::string>(pTree, "issuer_id");       // will use issuer's credit spread
	m_issuerIDType       = pt_get<std::string>(pTree, "issuer_id_type");

	if(m_isCallable)
	{   // In the xml there is a counting base of 1, in the calculator we use base 0, hence the '- 1'
		m_firstPeriodIssuersCallIsActive = pt_get<Integer>(pTree, "first_period_issuers_call_active");
		QL_REQUIRE(m_firstPeriodIssuersCallIsActive >= 1,
			       "CallSpreadCpnNoteContract: 'first_period_issuers_call_active' must be greater than\n"
				   << "or equal to 1, here it is: " << m_firstPeriodIssuersCallIsActive);

		m_firstPeriodIssuersCallIsActive--; // switching from base 1 in the xml to base 0 counting in this C++ code.
	}
	else // is not callable
		m_firstPeriodIssuersCallIsActive = -1; 

	m_couponDetailsPTree = pTree.get_child("coupon_details");
	pt_getVector<std::string>(pTree, "holiday_calendars", "id",    // inputs
                              m_holCalIDs);                        // outputs
}

bool CallSpreadCpnNoteContract::isCallable(Size period) const
{
	return (m_isCallable && ( (Integer)period >= m_firstPeriodIssuersCallIsActive));
}

class RawCouponSpecification
{
public:
	Size       m_startPeriod; // will use 0 base, unlike the xml data that uses 1 base

	bool       m_endPeriodIsAtMaturity;
	Size       m_endPeriod;

	bool       m_hasCap;
	Real       m_cap;

	bool       m_hasFloor;
	Real       m_floor;

	Real       m_factor;
	Real       m_strike;

	Real       m_capIncrement;
	Real       m_floorIncrement;
	Real       m_factorIncrement;
	Real       m_strikeIncrement;	

	RawCouponSpecification(const boost::property_tree::ptree& pTree);
};

RawCouponSpecification::RawCouponSpecification(const boost::property_tree::ptree& pTree)
{
	m_startPeriod = pt_get<Size>(pTree, "start_period");
	QL_REQUIRE(m_startPeriod > 0, "RawCouponSpecification: please use base 1 for start_period ("
		                          << m_startPeriod << ").");
	m_startPeriod--; // converting from base 1 to base 0.

	std::string endPeriodStr = pt_get<std::string>(pTree, "end_period");
	if(! (m_endPeriodIsAtMaturity = !strcmp("maturity", endPeriodStr.c_str())))
	{
       m_endPeriod = atoi(endPeriodStr.c_str());
	   QL_REQUIRE(m_endPeriod != 0, "RawCouponSpecification: end_period should either be an positive integer"
		                            << "\n(using base 1 counting) or the string 'maturity'."
									<< "\nHere it is: " << endPeriodStr);
	   m_endPeriod--; // converting from base 1 to base 0
	}
	std::string capStr = pt_get<std::string>(pTree, "cap");
	if( m_hasCap = (strcmp(capStr.c_str(), "na") != 0))
		m_cap = pt_get<Real>(pTree, "cap"); // we don't use atof(capStr) because that doesn't throw an error on failure
 
	std::string floorStr = pt_get<std::string>(pTree, "floor");
	if( m_hasFloor = (strcmp(floorStr.c_str(), "na") != 0))
		m_floor = pt_get<Real>(pTree, "floor"); // we don't use atof(floorStr) because that doesn't throw an error on failure

	std::string factorStr = pt_get<std::string>(pTree, "factor");
	if( !strcmp(factorStr.c_str(), "na"))
	{
		QL_REQUIRE(m_hasFloor && m_hasCap && (m_cap == m_floor),
			       "RawCouponSpecification: when the factor is set to 'na',\nneed the cap ("
				   << (m_hasCap   ? toString(m_cap)   : "not_set") << ") to equal the floor (" 
				   << (m_hasFloor ? toString(m_floor) : "not_set") << ").");
		m_factor = 0;
	}
	else
		m_factor = pt_get<Real>(pTree, "factor");

	std::string strikeStr = pt_get<std::string>(pTree, "strike");
	if( !strcmp(strikeStr.c_str(), "na"))
	{
		QL_REQUIRE(m_hasFloor && m_hasCap && (m_cap == m_floor),
			       "RawCouponSpecification: when the strike is set to 'na',\nneed the cap ("
				   << (m_hasCap   ? toString(m_cap)   : "not_set") << ") to equal the floor (" 
				   << (m_hasFloor ? toString(m_floor) : "not_set") << ").");
		m_strike = 0;
	}
	else
		m_strike = pt_get<Real>(pTree, "strike"); // we don't use atof(strikeStr) because that doesn't throw an error.

	m_capIncrement    = pt_get<Real>(pTree, "cap_increment");
	m_floorIncrement  = pt_get<Real>(pTree, "floor_increment");
	m_factorIncrement = pt_get<Real>(pTree, "factor_increment");
	m_strikeIncrement = pt_get<Real>(pTree, "strike_increment");	
}

class CallSpreadCpnNoteInstrument 
{
public:
	CallSpreadCpnNoteContract*                               m_CSCNC;
	MarketCaches*                                            m_marketCaches;
	std::vector<boost::shared_ptr<RawCouponSpecification> >  m_vRawCouponSpecs;
	const boost::shared_ptr<PricingEngine>                   m_pricingEngine;
	Date                                                     m_evalDate;
	Size                                                     m_treeTimeSteps;

	void setDates();
	void setRawCouponSpecs();
	void setCoupons();
	void setCurrentSpot();
	Size getIndexOfRawCouponSpec(Size period);

public:
	std::vector<Date>              m_periodEndDates;
	Size                           m_numPeriods;
	Size                           m_currentPeriod;
	boost::shared_ptr<Calendar>    m_settleCal;
	std::vector<CallSpreadCoupon>  m_coupons;
	Real                           m_currentSpot;
	
	boost::optional<Real>          m_npv;  // net present value

	CallSpreadCpnNoteInstrument(CallSpreadCpnNoteContract* pCSCNC, MarketCaches* pMarketCaches);

	Real getPayoffAtMaturity()                    const;
	Real getPaymentIfCalled()                     const;
	Real getCouponPayment(Size period, Real spot) const;
	Real getNPV()                                 const;
	void Calculate(); // will calc the NPV
};

class DiscretizedCSCN : public DiscretizedAsset
{
private:
	CallSpreadCpnNoteInstrument *m_CSCNI;
    boost::shared_ptr<GeneralizedBlackScholesProcess> m_process;
	std::vector<Time> m_adjustedPeriodEndTimes; // future only, on the grid
public:
	DiscretizedCSCN(CallSpreadCpnNoteInstrument*                       pCSCNI, 
                    boost::shared_ptr<GeneralizedBlackScholesProcess>  process,
				    const TimeGrid&                                    timeGrid);

	void reset(Size size); // set the payments at maturity

	std::vector<Time> mandatoryTimes() const;
    void postAdjustValuesImpl();
    void dealWithPeriodEndDate(Size futurePeriod);
};

void DiscretizedCSCN::postAdjustValuesImpl()
{
    for (Size futurePeriod = 0; futurePeriod < m_adjustedPeriodEndTimes.size(); futurePeriod++) 
	{
        if (isOnTime(m_adjustedPeriodEndTimes[futurePeriod]))
           dealWithPeriodEndDate(futurePeriod);
    }

}	

void DiscretizedCSCN::dealWithPeriodEndDate(Size futurePeriod)
{
	writeDiagnostics("In tree, dealing with future period: " + toString(futurePeriod),
		             high, "DiscretizedCSCN::dealWithPeriodEndDate");

    Array underlyingPrices = method()->grid(time());
	Size period = futurePeriod + m_CSCNI->m_currentPeriod;
    for (Size j=0; j<values_.size(); j++) 
	{
		// we make the decision whether or not to call the instrument 
		// before we add the coupon payment to the continuation value.
		// i.e. here time is going backwards.
		// In the real world when time goes forward,
		// we pay the coupon first, then make the decision about whether
		// or not to call.

		if(m_CSCNI->m_CSCNC->isCallable(period)) // we have an issuer's call
		   values_[j] = std::min(m_CSCNI->m_CSCNC->m_rebate, values_[j]); 
		// else  don't have issuer's call so leave values as it is

	    Real couponPayment = m_CSCNI->getCouponPayment(period, underlyingPrices[j]);
		values_[j] = couponPayment + values_[j]; // pay the coupon
	}	 
}

std::vector<Time> DiscretizedCSCN::mandatoryTimes() const 
{
    return m_adjustedPeriodEndTimes; // future only
}

// Set the values at maturity:
void DiscretizedCSCN::reset(Size size)
{
    values_ = Array(size, m_CSCNI->m_CSCNC->m_rebate);
	dealWithPeriodEndDate(m_adjustedPeriodEndTimes.size()-1); // pay final coupon
}

DiscretizedCSCN::DiscretizedCSCN(CallSpreadCpnNoteInstrument*                       pCSCNI, 
                                 boost::shared_ptr<GeneralizedBlackScholesProcess>  process,
								 const TimeGrid&                                    timeGrid) 
{
	m_CSCNI   = pCSCNI;
	m_process = process;

	for(Size period = m_CSCNI->m_currentPeriod; period < m_CSCNI->m_numPeriods; period++)
	{
		Time unadjustedPeriodEnd = (m_CSCNI->m_periodEndDates[period] - m_CSCNI->m_evalDate)/365.0;
		m_adjustedPeriodEndTimes.push_back( timeGrid.closestTime(unadjustedPeriodEnd));
	}
}

Real CallSpreadCpnNoteInstrument::getNPV() const
{
	QL_REQUIRE( m_npv, "CallSpreadCpnNoteInstrument::getNPV(): Have not yet calculated npv.");
	return m_npv.get(); 
}

void CallSpreadCpnNoteInstrument::Calculate()
{
	// stoch process
    Handle<Quote> underlyingH(boost::shared_ptr<Quote>(new SimpleQuote(m_currentSpot)));
	Handle<YieldTermStructure> yieldTSAccCcy (m_marketCaches->getYieldTSCache()->
		                  get( CONST_STR_risk_free_rate, m_CSCNC->m_accCcy));
	Handle<YieldTermStructure> yieldTSUndlCcy (m_marketCaches->getYieldTSCache()->
		                  get( CONST_STR_risk_free_rate, m_CSCNC->m_undlCcy));

	Handle<BlackVolTermStructure> fxVolTS(m_marketCaches->getFXVolCache()
		                                  ->get(m_CSCNC->m_accCcy, m_CSCNC->m_undlCcy));

    boost::shared_ptr<StochasticProcess1D> stochasticPro(new BlackScholesMertonProcess
		(underlyingH, yieldTSUndlCcy, yieldTSAccCcy, fxVolTS));
       
    Time maturity = yieldTSAccCcy->dayCounter().yearFraction(m_evalDate, m_periodEndDates[m_numPeriods-1]);

    boost::shared_ptr<GeneralizedBlackScholesProcess> bs(
         new GeneralizedBlackScholesProcess(underlyingH, yieldTSUndlCcy, yieldTSAccCcy, fxVolTS));

	boost::shared_ptr<JarrowRudd> tree(new JarrowRudd(bs, maturity, m_treeTimeSteps, 0));

	// issuer's credit spread:
    Real creditSpread = m_marketCaches->getStockDataCache()->get(m_CSCNC->m_issuerID, m_CSCNC->m_issuerIDType)
		                  ->getCreditSpread();

    Volatility vol = fxVolTS->blackVol(m_periodEndDates[m_numPeriods-1], m_currentSpot);

	DayCounter rfdc  = yieldTSAccCcy->dayCounter();

	Rate riskFreeRate = yieldTSAccCcy->zeroRate(m_periodEndDates[m_numPeriods-1], rfdc, Continuous, NoFrequency);
    Rate q = yieldTSUndlCcy->zeroRate(m_periodEndDates[m_numPeriods-1], rfdc, Continuous, NoFrequency);

    boost::shared_ptr<Lattice> lattice(new BlackScholesLattice<JarrowRudd>
		(tree,riskFreeRate + creditSpread, maturity, m_treeTimeSteps));
    
    DiscretizedCSCN discretizedCSCN(this, bs, lattice->timeGrid()); //, TimeGrid(maturity, m_treeTimeSteps));

    discretizedCSCN.initialize(lattice, maturity);
    discretizedCSCN.rollback(0.0);
    m_npv = discretizedCSCN.presentValue();
}

void CallSpreadCpnNoteInstrument::setCurrentSpot()
{
	m_currentSpot = m_marketCaches->getFXPricesCache()->get(m_CSCNC->m_undlCcy, m_CSCNC->m_accCcy)
	                  ->getCurrentPrice();
}

Real CallSpreadCpnNoteInstrument::getPayoffAtMaturity() const
{
	return m_CSCNC->m_rebate;
}

Real CallSpreadCpnNoteInstrument::getPaymentIfCalled() const
{
	return m_CSCNC->m_rebate;
}

Real CallSpreadCpnNoteInstrument::getCouponPayment(Size period, Real spot) const
{   
	return m_coupons[period].getCouponRate(spot) * (Real) m_CSCNC->m_monthsPerPeriod / 12.0;
}

Size CallSpreadCpnNoteInstrument::getIndexOfRawCouponSpec(Size period)
{
	QL_REQUIRE(m_vRawCouponSpecs.size() > 0, "CallSpreadCpnNoteInstrument::getIndexOfRawCouponSpec: " 
		       << "\nMust have at least one raw coupon specification set.");

	bool alreadyFound = false;
	Size indexOfRawCouponSpec; 
	for(Size specNum = 0; specNum < m_vRawCouponSpecs.size(); specNum++)
	{
		if( (period >= m_vRawCouponSpecs[specNum]->m_startPeriod)
			&& (    m_vRawCouponSpecs[specNum]->m_endPeriodIsAtMaturity 
			     || (period <= m_vRawCouponSpecs[specNum]->m_endPeriod )))
		{
			QL_REQUIRE(!alreadyFound, "CallSpreadCpnNoteInstrument::getIndexOfRawCouponSpec(.):"
				       << "\nPeriod " << period+1 << " is covered by more than one raw coupon spec.");
			indexOfRawCouponSpec = specNum;
			alreadyFound = true;
		}
	}     
	QL_REQUIRE(alreadyFound, "CallSpreadCpnNoteInstrument::getIndexOfRawCouponSpec(.):"
		       "\nWas unable to find a raw coupon spec for period " << period+1);

	return indexOfRawCouponSpec;
}

CallSpreadCpnNoteInstrument::CallSpreadCpnNoteInstrument(CallSpreadCpnNoteContract* pCSCNC, MarketCaches* pMarketCaches)
{
	m_CSCNC         = pCSCNC;
    m_marketCaches  = pMarketCaches;
	m_evalDate      = m_marketCaches->getEvalDate();

	m_treeTimeSteps = atoi(getConfig()->get("call_spread_cpn_note_tree_time_steps").c_str());
	QL_REQUIRE(m_treeTimeSteps > 10, "Must have more than 10 time steps in the tree.\n"
		       << "Found call_spread_cpn_note_tree_time_steps to be: " 
		       << getConfig()->get("call_spread_cpn_note_tree_time_steps"));

	setDates();
	setCoupons();
	setCurrentSpot(); 
}

void CallSpreadCpnNoteInstrument::setDates()
{
    m_settleCal = getHolCal(m_CSCNC->m_holCalIDs, m_marketCaches, 
		                    m_CSCNC->m_startDate, m_CSCNC->m_finalObsDate + 100);

    Schedule schedule(m_CSCNC->m_startDate, m_CSCNC->m_finalObsDate,
                      Period(m_CSCNC->m_monthsPerPeriod, Months), *m_settleCal, 
					  Following, Following, DateGeneration::Backward, false);

	// we exclude the first element
	m_periodEndDates = subset<Date>(schedule.dates(), 1, schedule.dates().size()-1);    
	m_numPeriods     = m_periodEndDates.size();

	Size period =0;
	while(m_periodEndDates[period] < m_marketCaches->getEvalDate())
	{
		period++;
		QL_REQUIRE(period < m_numPeriods, "CallSpreadCpnNoteInstrument::setDates():"
			       << "\nThe trade has expired. Eval Date is: " << toString(m_marketCaches->getEvalDate())
			       << "\nand the last period end date is:     " << toString(m_periodEndDates[m_numPeriods-1]));
	}
	m_currentPeriod = period;
}

void CallSpreadCpnNoteInstrument::setRawCouponSpecs()
{
	boost::property_tree::ptree::const_iterator iter = m_CSCNC->m_couponDetailsPTree.begin();

	while( iter != m_CSCNC->m_couponDetailsPTree.end())
	{
		if(iter->first.data() == CONST_STR_coupon_specification)
		{
			boost::shared_ptr<RawCouponSpecification> rawCpnSpec = (boost::shared_ptr<RawCouponSpecification>)
				       new RawCouponSpecification(iter->second);
			m_vRawCouponSpecs.push_back(rawCpnSpec);
		}
		else
		{
			writeDiagnostics("Found unknown node: " + toString(iter->first.data())
				             + "\nwith details:\n" + toString(iter->second), 
			                 low, "CallSpreadCpnNoteInstrument::setCoupons");
		}
		iter++;
	}
}

void CallSpreadCpnNoteInstrument::setCoupons()
{
	setRawCouponSpecs();
	m_coupons.resize(m_numPeriods);

	Size indexOfRawCouponSpec = 999999; 
	Size stepNumber           = 0;
	for(Size period = 0; period < m_numPeriods; period++)
	{
   	    Size previousIndexOfRaw   = indexOfRawCouponSpec;
		indexOfRawCouponSpec      = getIndexOfRawCouponSpec(period);
		if(indexOfRawCouponSpec  != previousIndexOfRaw)
			stepNumber = 0; // step number gets reset to zero
		else
			stepNumber++;

		boost::shared_ptr<RawCouponSpecification> rawSpec = m_vRawCouponSpecs[indexOfRawCouponSpec];

		Real factor = rawSpec->m_factor +  stepNumber * (rawSpec->m_factorIncrement);
		Real strike = rawSpec->m_strike +  stepNumber * (rawSpec->m_strikeIncrement);

		boost::optional<Real> cap; 
		if(rawSpec->m_hasCap)
			cap = rawSpec->m_cap + stepNumber * (rawSpec->m_capIncrement);
		else
			cap.reset(); // want cap to  be in the 'uninitialized' state.

		boost::optional<Real> floor;
		if(rawSpec->m_hasFloor)
			floor = rawSpec->m_floor + stepNumber * (rawSpec->m_floorIncrement);
		else
			floor.reset(); // want floor to be the 'uninitialized' state

		m_coupons[period].reset(factor, strike, cap, floor);
	}
}
 
CallSpreadCpnNoteCalculator::CallSpreadCpnNoteCalculator(CallSpreadCpnNoteContract* pCSCNC,       // input
														 MarketCaches*              pMarketCaches,// input
		                                                 ResultSet*                 pResultSet)   // output
														 // constructor will set pResultSet
					 : CalculatorBase(pCSCNC, pMarketCaches, pResultSet)
{
	boost::shared_ptr<CallSpreadCpnNoteInstrument> CSCNI = (boost::shared_ptr<CallSpreadCpnNoteInstrument>)
		        new CallSpreadCpnNoteInstrument(pCSCNC, pMarketCaches);

	CSCNI->Calculate();

	/////////////////////////////////////////////////////////////////////////////
	// populate results
	boost::shared_ptr<Result> perUnitValRes = (boost::shared_ptr<Result>) new Result();
	perUnitValRes->setAttribute ( contract_category, "call_spread_cpn_note",                true);
	perUnitValRes->setAttribute ( contract_id,       pCSCNC->getID(),                       true);
	perUnitValRes->setAttribute ( underlying_id,     pCSCNC->m_undlCcy + pCSCNC->m_accCcy , true);
	perUnitValRes->setValueAndCategory ( price_per_unit_notional, CSCNI->getNPV());
	pResultSet->addNewResult           ( perUnitValRes);		
}
