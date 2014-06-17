#include "ConvertibleBond.hpp"
#include "MarketData.hpp"
#include "Result.hpp"

ConvertibleBondContract::ConvertibleBondContract(const boost::property_tree::ptree &pt)
		: Contract( convertible_bond, pt_get<std::string>(pt, "contract_id"))
{
	boost::property_tree::ptree pTree = pt.get_child("convertible_bond");

	m_issueDate             = stringToDate( pt_get<std::string>(pTree, "issue_date")         , getConfig()->getDateFormat());
	m_exerciseDate          = stringToDate( pt_get<std::string>(pTree, "final_exercise_date"), getConfig()->getDateFormat());

	m_monthBetweenCoupons   = pt_get<Integer>             (pTree, "month_between_coupons" );
	m_coupon                = pt_get<Real>                (pTree, "coupon"                );
	m_redemption            = pt_get<Real>                (pTree, "redemption"            );
	m_currency              = pt_get<std::string>         (pTree, "currency"              );
	m_numberOfBonds         = pt_get<Real>                (pTree, "number_of_bonds"       );
	m_conversionRatio       = pt_get<Real>                (pTree, "conversion_ratio"      );
    m_underlyingStockID     = pt_get<std::string>         (pTree, "underlying_stock_id"   );
	m_stockIDType           = pt_get_optional<std::string>(pTree, "stock_id_type", ""     );

	QL_REQUIRE( m_redemption > 0, "ConvertibleBondContract::ConvertibleBondContract(..): redemption must be strictly greater than zero. "
                                  << " Here it is " << m_redemption);
}


// The constructor does the work to generate the results.
ConvertibleBondCalculator::ConvertibleBondCalculator(
	         ConvertibleBondContract* pCBondContract, MarketCaches* pMarketCaches,
		     ResultSet* pResultSet) // constructor will set the resultSet
				  : CalculatorBase(pCBondContract, pMarketCaches, pResultSet)
{
	m_CBondContract = pCBondContract;
	setStockData(m_CBondContract->m_underlyingStockID, m_CBondContract->m_stockIDType);
		
	QL_REQUIRE(m_stockData->getCurrency() == m_CBondContract->m_currency,
		       "ConvertibleBondCalculator::ConvertibleBondCalculator(..): "
		       << "Model only deals with case when the stock currency (" << m_stockData->getCurrency()
			   << ") is the same as the notional currency (" << m_CBondContract->m_currency
			   << ")"); // could extend to deal with quanto.

	boost::shared_ptr<Prices> prices = m_marketCaches->getStockPricesCache()
		                                             ->get(m_CBondContract->m_underlyingStockID,
		                                                   m_CBondContract->m_stockIDType);
    Option::Type type(Option::Put);
    Real underlying     = prices->getCurrentPrice();
	Real spreadRate     = m_stockData->getCreditSpread(); 
    Real dividendYield  = m_stockData->getDividendYield();

	Handle<YieldTermStructure> yieldTS (pMarketCaches->getYieldTSCache()->get( CONST_STR_risk_free_rate, 
		                                                                       m_CBondContract->m_currency));
    Real    redemption      = m_CBondContract->m_redemption;
	Real    conversionRatio = m_CBondContract->m_conversionRatio;

    // set up dates/schedules
    Calendar calendar = TARGET();

    Settings::instance().evaluationDate() = m_marketCaches->getEvalDate();
    Date settlementDate = m_marketCaches->getEvalDate();

    Date issueDate    = m_CBondContract->m_issueDate;
    Date exerciseDate = m_CBondContract->m_exerciseDate;

    BusinessDayConvention convention = ModifiedFollowing;
    Frequency frequency = monthsInPeriodToFrequencyEnum(m_CBondContract->m_monthBetweenCoupons);

    Schedule schedule(issueDate, exerciseDate,
                          Period(frequency), calendar,
                          convention, convention,
                          DateGeneration::Backward, false);

    DividendSchedule dividends;
    CallabilitySchedule callability;
	Integer numCoupons = schedule.size() - 1; 

	std::vector<Real> coupons(schedule.size()-1, m_CBondContract->m_coupon);

    DayCounter bondDayCount = Thirty360();

    DayCounter dayCounter = Actual365Fixed();
    Time maturity = dayCounter.yearFraction(settlementDate, exerciseDate);

    boost::shared_ptr<Exercise> exercise(
                                      new EuropeanExercise(exerciseDate));

    Handle<Quote> underlyingH(
        boost::shared_ptr<Quote>(new SimpleQuote(underlying)));

    Handle<YieldTermStructure> flatDividendTS(
        boost::shared_ptr<YieldTermStructure>(
            new FlatForward(settlementDate, dividendYield, dayCounter)));

    Real volatility = m_stockData->getFlatVol();
	Handle<BlackVolTermStructure> flatVolTS(
        boost::shared_ptr<BlackVolTermStructure>(
            new BlackConstantVol(settlementDate, calendar,
                                 volatility, dayCounter)));

    boost::shared_ptr<BlackScholesMertonProcess> stochasticProcess(
                          new BlackScholesMertonProcess(underlyingH,
                                                        flatDividendTS,
                                                        yieldTS,
                                                        flatVolTS));

    Size timeSteps = atoi(getConfig()->get("convertible_bond_time_steps").c_str());

    Handle<Quote> creditSpread(
                   boost::shared_ptr<Quote>(new SimpleQuote(spreadRate)));

	Real riskFreeRate = yieldTS->zeroRate(1, Compounded);
    boost::shared_ptr<Quote> rate(new SimpleQuote(riskFreeRate));

    Handle<YieldTermStructure> discountCurve(
            boost::shared_ptr<YieldTermStructure>(
                new FlatForward(m_marketCaches->getEvalDate(), Handle<Quote>(rate), dayCounter)));

    Integer settlementDays = 0;
    ConvertibleFixedCouponBond europeanBond(
                        exercise, conversionRatio, dividends, callability,
                        creditSpread, issueDate, settlementDays,
                        coupons, bondDayCount, schedule, redemption);

	std::string method = "Jarrow-Rudd tree";
    europeanBond.setPricingEngine(boost::shared_ptr<PricingEngine>(
              new BinomialConvertibleEngine<JarrowRudd>(stochasticProcess,
                                                        timeSteps)));

	/////////////////////////////////////////////////////////////////////////////
	boost::shared_ptr<Result> res = (boost::shared_ptr<Result>) new Result();
		
	res->setValueAndCategory(price_per_unit_notional, europeanBond.NPV() / m_CBondContract->m_redemption);

	res->setAttribute(contract_category, "convertible_bond",                   true);
	res->setAttribute(contract_id,       m_CBondContract->getID(),             true);
	res->setAttribute(underlying_id,     m_CBondContract->m_underlyingStockID, true);
	res->setAttribute(eval_date,         toString(pMarketCaches->getEvalDate(), 
		                                              getConfig()->getDateFormat()));
	pResultSet->addNewResult(res);

	boost::shared_ptr<Result> fullPriceRes = (boost::shared_ptr<Result>) new Result(*res);
	fullPriceRes->setValueAndCategory(cash_price, europeanBond.NPV() * m_CBondContract->m_numberOfBonds);
	fullPriceRes->setAttribute(currency, m_CBondContract->m_currency,  true);
	pResultSet->addNewResult(fullPriceRes);
}
