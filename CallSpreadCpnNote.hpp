#ifndef callspreadcpnnote_hpp
#define callspreadcpnnote_hpp

#include "Utilities.hpp"
#include "Portfolio.hpp"

// The coupon paid will be: min( cap, max( floor, underlying * factor - strike))
// For a put-spread set the factor and strike to be negative.
// A call-spread coupon is the same as a 'collar'.
// both are equivalent to:   long the underlying, long put with X, short call with strike X+A
// with is also equivalent to: long call with strike X, short a call with strike X+A
class CallSpreadCoupon
{
private:
    Real   m_factor;
	Real   m_strike;

	bool   m_capIsActive;
	Real   m_cap;

    bool   m_floorIsActive;
	Real   m_floor;

	bool   m_initialized;
public:
	CallSpreadCoupon(Real factor,                  Real strike, 
		             boost::optional<Real> cap,    boost::optional<Real> floor);

	CallSpreadCoupon();

	void reset(Real factor,                  Real strike, 
               boost::optional<Real> cap,    boost::optional<Real> floor);

	Real getCouponRate(Real spot) const;     // will throw if uninitialized
};

class CallSpreadCpnNoteContract : public Contract
{
public:
	Date                          m_startDate;
	Date                          m_finalObsDate;
	bool                          m_isCallable;
	Integer                       m_firstPeriodIssuersCallIsActive; 
	Real                          m_rebate;                      // the rebate per unit notional when called or at maturity
	Size                          m_monthsPerPeriod;
	
	std::string                   m_underlyingType;
	std::string                   m_accCcy; // aka payout currency
	std::string                   m_undlCcy;
	
	std::vector<std::string>      m_holCalIDs;
	Size                          m_settlementLag;
	boost::property_tree::ptree   m_couponDetailsPTree;

	std::string                   m_issuerID;     // will use issuer's credit spread
	std::string                   m_issuerIDType;

	bool isCallable(Size period) const;
    CallSpreadCpnNoteContract(const boost::property_tree::ptree &parentTree); 
};

class CallSpreadCpnNoteCalculator : public CalculatorBase
{
private:
    
public:
	CallSpreadCpnNoteCalculator(CallSpreadCpnNoteContract* pCSCNC, MarketCaches* pMarketCaches,
		                        ResultSet* pResultSet); // constructor will set the resultSet
};

#endif // #ifndef callspreadcpnnote_hpp
