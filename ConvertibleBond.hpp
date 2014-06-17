#ifndef convertiblebond_hpp
#define convertiblebond_hpp

#include "Utilities.hpp"
#include "Portfolio.hpp"

class ConvertibleBondContract : public Contract
{
public:
	Date          m_issueDate;
	Date          m_exerciseDate;
	Integer       m_monthBetweenCoupons;
	Real          m_coupon;

	Real          m_redemption;
	std::string   m_currency;
	Real          m_numberOfBonds;
	Real		  m_conversionRatio;

	std::string   m_underlyingStockID;
	std::string   m_stockIDType;

	ConvertibleBondContract(const boost::property_tree::ptree &pt);
};

class ConvertibleBondCalculator : public CalculatorBase
{
private:
	ConvertibleBondContract* m_CBondContract;
public:
	ConvertibleBondCalculator(ConvertibleBondContract* pCBondContract, MarketCaches* pMarketCaches,
		                      ResultSet* pResultSet); // constructor will set the resultSet
};

#endif 