#ifndef equitylinkednote_hpp
#define equitylinkednote_hpp

#include "Utilities.hpp"
#include "Portfolio.hpp"

// The EquityLinkedNote that is valued here has 
//       payoff = notional * max(1, (S[T] - k)/k)
// So it can be considered to be (1 - (put)/k)
class EquityLinkedNoteContract : public Contract
{
public:
	Date          m_startDate;
	Date          m_finalObservation;
	Date          m_finalSettle;
	Real          m_strikePrice;   // quoted in units of the currency of the stock
	Real          m_notional;
	std::string   m_notionalCurrency;
	std::string   m_underlyingStockID;
	std::string   m_underlyingStockIDType;

	EquityLinkedNoteContract(const boost::property_tree::ptree &pt);
};

class EquityLinkedNoteCalculator : public CalculatorBase
{
public:
	EquityLinkedNoteCalculator(EquityLinkedNoteContract* pELNContract, MarketCaches* pMarketCaches,
		                      ResultSet* pResultSet); // constructor will set the resultSet
};

#endif


