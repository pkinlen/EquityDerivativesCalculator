#ifndef accumulator_hpp
#define accumulator_hpp

#include "Utilities.hpp"
#include "Portfolio.hpp"

enum AccumSubCategory
{
    accumulator_note,
	accumulator_swap,
	decumulator_swap
};

class AccumulatorContract : public Contract
{
public:
	Date                m_issueDate;
	Date                m_firstAccumDate;
	Real                m_strikePrice;
	Real                m_refSpot;
	Real                m_KOPrice;
	bool                m_KOSettleIsAtPeriodEnd;

	Real                m_sharesPerDay; // can be non-integer.
    Real                m_positionSize; // if you hold 4.5 accumulators it will be 4.5! Can also be negative for a short position.

	AccumSubCategory    m_subCategory;  // i.e accumulator_note, accumulator_swap or decumulator_swap

	std::string         m_underlyingID;
	std::string         m_underlyingIDType;

	Integer             m_settleLag;

	bool                m_hasGuaranteedAccum;
	Date                m_lastGuaranteedAccum;
    Date                m_firstKODate;

    Size                m_numPeriods;

    bool                m_hasGearing;
    Real                m_gearingStrike;
    Real                m_maxGearingMultiplier;    // expected to be 1 (when no gearing) or 2 with gearing
	Real                m_gearingMultiplier;
	Real                m_maxGearingTimesStrike;   // rather than do the same multiplication many times in the 
	Real                m_sharesPerDayTimesStrike; // MC calculator we do it once here.
    
	std::vector<Date>   m_periodEndDates;          // size m_numPeriods

	AccumulatorContract(const boost::property_tree::ptree &pt);
	void initialize    (const boost::property_tree::ptree &pt);
};

class AccumulatorCalculator : public CalculatorBase
{
private:
    
public:
	AccumulatorCalculator(AccumulatorContract* pAccumContract, MarketCaches* pMarketCaches,
		                  ResultSet* pResultSet); // constructor will set the resultSet
};

#endif 