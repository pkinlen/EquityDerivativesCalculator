#ifndef rangeaccrual_hpp
#define rangeaccrual_hpp

#include "Utilities.hpp"
#include "Portfolio.hpp"
#include "Result.hpp"


class RangeAccrualContract : public Contract
{
public:
	std::string               m_underlyingType;
	std::string               m_accCcy; // aka payout currency
	std::string               m_undlCcy;
	Date                      m_firstAccrualDate;
	Date                      m_maturity;
	Size                      m_monthsPerPeriod;
	Real                      m_couponInitial;
	Real                      m_couponStep;
	Real                      m_lowBarrierInitial;
	Real                      m_lowBarrierStep;
	Real                      m_redemptionPerUnitNotional;
	Real                      m_notional;
	std::vector<std::string>  m_holCalIDs;
	Size                      m_settlementLag;

    RangeAccrualContract(const boost::property_tree::ptree &parentTree); 
};

class RangeAccrualCalculator : public CalculatorBase
{
private:
    
public:
	RangeAccrualCalculator(RangeAccrualContract* pRangeAccrualContract, MarketCaches* pMarketCaches,
		                   ResultSet* pResultSet); // constructor will set the resultSet
};


#endif // for #ifndef rangeaccrual_hpp
