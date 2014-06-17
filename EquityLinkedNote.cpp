#include "EquityLinkedNote.hpp"
#include "MarketData.hpp"
#include "Result.hpp"

// The constructor does the work to generate the results.
EquityLinkedNoteCalculator::EquityLinkedNoteCalculator(
	         EquityLinkedNoteContract* pELNContract, MarketCaches* pMarketCaches,
		     ResultSet* pResultSet) // constructor will set the resultSet
				  : CalculatorBase(pELNContract, pMarketCaches, pResultSet)
{
	boost::shared_ptr<StockData> stockData = pMarketCaches->getStockDataCache()->get(
		                                                         pELNContract->m_underlyingStockID,
		                                                         pELNContract->m_underlyingStockIDType);
		
	QL_REQUIRE(stockData->getCurrency() == pELNContract->m_notionalCurrency,
		       "EquityLinkedNoteCalculator::EquityLinkedNoteCalculator(..): "
		       << "Model only deals with case when the stock currency (" << stockData->getCurrency()
			   << ") is the same as the notional currency (" << pELNContract->m_notionalCurrency
			   << ")"); // could extend to deal with quanto. Note that physical settlement isn't quanto!

	boost::shared_ptr<Exercise> europeanExercise(new EuropeanExercise(pELNContract->m_finalObservation));
 
	boost::shared_ptr<Prices> prices = m_marketCaches->getStockPricesCache()
		                                             ->get(pELNContract->m_underlyingStockID,
		                                                   pELNContract->m_underlyingStockIDType);

	Handle<Quote> underlyingH(boost::shared_ptr<Quote>(new SimpleQuote(prices->getCurrentPrice())));
 
	boost::shared_ptr<CacheDualKey<boost::shared_ptr<YieldTermStructure> > > 
			yieldTSCache = pMarketCaches->getYieldTSCache(); 

	Handle<YieldTermStructure> yieldTS (yieldTSCache->get( CONST_STR_risk_free_rate, 
		                                                   pELNContract->m_notionalCurrency));
  
	Handle<YieldTermStructure> flatDividendTS(
           boost::shared_ptr<YieldTermStructure>(
               new FlatForward(pMarketCaches->getEvalDate() , 
				               stockData->getDividendYield() , 
				               Actual365Fixed())));

    Handle<BlackVolTermStructure> flatVolTS (
            boost::shared_ptr<BlackVolTermStructure>(
                new BlackConstantVol(pMarketCaches->getEvalDate(), TARGET(), stockData->getFlatVol(),
                                     Actual365Fixed())));

	boost::shared_ptr<StrikedTypePayoff> payoff(
                new PlainVanillaPayoff(Option::Put, pELNContract->m_strikePrice));

    boost::shared_ptr<BlackScholesMertonProcess> bsmProcess(
                new BlackScholesMertonProcess(underlyingH, flatDividendTS, yieldTS, flatVolTS));

    VanillaOption europeanOption(payoff, europeanExercise);

    europeanOption.setPricingEngine(boost::shared_ptr<PricingEngine>(
                                     new AnalyticEuropeanEngine(bsmProcess)));

	/////////////////////////////////////////////////////////////////////////////
	boost::shared_ptr<Result> res = (boost::shared_ptr<Result>) new Result();
		
	Real strikeDivisor      = 1.0 / ( pELNContract->m_strikePrice == 0.0 ? 1.0 : pELNContract->m_strikePrice);
	Real discFact           = yieldTS->discount( pELNContract->m_finalObservation); 
	Real valPerUnitNotional = discFact - europeanOption.NPV() * strikeDivisor;
	res->setValueAndCategory(price_per_unit_notional, valPerUnitNotional);

	res->setAttribute(contract_category, "equity_linked_note",              true);
	res->setAttribute(contract_id,       pELNContract->getID(),             true);
	res->setAttribute(underlying_id,     pELNContract->m_underlyingStockID, true);
	res->setAttribute(eval_date,         toString(pMarketCaches->getEvalDate(), 
		                                              getConfig()->getDateFormat()));

	pResultSet->addNewResult(res);

	boost::shared_ptr<Result> fullPriceRes = (boost::shared_ptr<Result>) new Result(*res);
	fullPriceRes->setValueAndCategory(cash_price, valPerUnitNotional * pELNContract->m_notional);
	fullPriceRes->setAttribute(currency, pELNContract->m_notionalCurrency,  true);
	pResultSet->addNewResult(fullPriceRes);

	// using the copy constructor for Result, saves writing out the code to set the attrs again.
	boost::shared_ptr<Result> deltaRes = (boost::shared_ptr<Result>) new Result(*res);
	Real delta = - europeanOption.delta() * prices->getCurrentPrice() * strikeDivisor;
	deltaRes->setValueAndCategory(delta_pc, delta);

	pResultSet->addNewResult(deltaRes);
}

EquityLinkedNoteContract::EquityLinkedNoteContract(const boost::property_tree::ptree &pt)
		: Contract( equity_linked_note, 
		            pt_get<std::string>(pt, "contract_id"))
{
	boost::property_tree::ptree elnPtree = pt.get_child("equity_linked_note");

	// It is not essential to have the start date and so we deem it to be an optional parameter
	boost::optional<std::string> startDateStr = elnPtree.get_optional<std::string>("start_date");
	if(startDateStr)
		m_startDate         = stringToDate( *startDateStr, getConfig()->getDateFormat());

	m_finalObservation      = stringToDate( pt_get<std::string>(elnPtree, "final_observation_date") , getConfig()->getDateFormat());
	m_finalSettle           = stringToDate( pt_get<std::string>(elnPtree, "final_settlement_date") ,  getConfig()->getDateFormat());

	m_strikePrice           = pt_get<Real>(elnPtree, "strike_price");
	m_notional              = pt_get<Real>(elnPtree, "notional");
	m_notionalCurrency      = pt_get<std::string>(elnPtree, "notional_currency");
    m_underlyingStockID     = pt_get<std::string>(elnPtree, "underlying_stock_id");
	m_underlyingStockIDType = pt_get_optional<std::string>(elnPtree, (std::string)"stock_id_type", (std::string)"");	
}
