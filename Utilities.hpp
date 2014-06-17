#ifndef utilities_h
#define utilities_h

/* Copyright of Sateek:
    see: http://sateek.com/?pg=strict_licence
	using the quantlib and boost libraries,
	see their (free-ware) copyright notices: boost.org & quantlib.org 
*/

// the only header you need to use QuantLib
#include <ql/quantlib.hpp>

#ifdef BOOST_MSVC
/* Uncomment the following lines to unmask floating-point
   exceptions. Warning: unpredictable Results can arise...

   See http://www.wilmott.com/messageview.cfm?catid=10&threadid=9481
   Is there anyone with a definitive word about this?
*/
// #include <float.h>
// namespace { unsigned int u = _controlfp(_EM_INEXACT, _MCW_EM); }
#endif


#include <boost/timer.hpp>
#include <iostream>
#include <iomanip>

using namespace QuantLib;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {

    Integer sessionId() { return 0; }

}
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <string>
#include <set>
#include <exception>
#include <iostream>
#include <fstream>

#ifdef __linux__
const std::string CONST_STR_config_xml                  = "/sateek/config.xml";
#else
const std::string CONST_STR_config_xml                  = "c:/sateek/config.xml"; 
#endif

const std::string CONST_STR_calendar                    = "calendar";
const std::string CONST_STR_contract                    = "contract";
const std::string CONST_STR_coupon_specification        = "coupon_specification";
const std::string CONST_STR_divider                     = "<divider>";
const std::string CONST_STR_fx_spot                     = "fx_spot";
const std::string CONST_STR_fx_vol                      = "fx_vol";
const std::string CONST_STR_item                        = "item";
const std::string CONST_STR_market_data_source_default  = "market_data_source_default";
const std::string CONST_STR_risk_free_rate              = "risk_free_rate";
const std::string CONST_STR_stock                       = "stock";
const std::string CONST_STR_version                     = "Sateek Calculator version 0.01";
const std::string CONST_STR_xmlcomment                  = "<xmlcomment>";
const std::string CONST_STR_xmlfile                     = "xmlfile";

const BigNatural CONST_defaultRandomGeneratorSeed       = 0;

enum ContractCategory
{
	barrier_option,
	call_spread_cpn_note,
	convertible_bond,
    equity_linked_note,
    koda,                 // Knock-out Accumulators and Decumulators
	range_accrual,
    vanilla_option     
};

std::string toString(ContractCategory e);

enum DiagnosticLevel // will be specified in the config
{
	none = 0,
	low  = 1,
	mid  = 2,
	high = 3,
	full = 4
};

const DiagnosticLevel CONST_defaultDiagnosticLevel = low;

DiagnosticLevel diagnosticLevelStringToEnum(const std::string& str);

std::string toString(const Date d, const std::string& format = "d-mmm-yyyy");
std::string toString(const boost::property_tree::ptree& pt);

template<class T>
std::string toString(T t)
{
	std::stringstream stream;
	stream  << t; // could have << std::setprecision(15)
	return stream.str();
}

template<class T>
std::string toString(std::vector<T> v, std::string separator = "\n", bool showIndex = false)
{
	std::string str;
	for(Size i = 0; i < v.size(); i++)
	{
		str += (showIndex ? toString(i+1) + ": " : "") + toString(v[i]) + separator;
	}
	return str;
}

// Convert string to Month enum
Month monthAsStringToEnum(const std::string& monthAsString);

// An alternative to this function is DateParser::parse(..) in dataparsers.cpp
// though it is not quite the same.
Date stringToDate(const std::string& dateAsString, const std::string& format);

class Config
{
private:
	std::map<std::string, std::string>        m_map;
	BigNatural                                m_randomGeneratorSeed;
	bool                                      m_randomGeneratorSeedIsInitialized;
	std::string                               m_dateFormat;

	// when throwIfAlreadyPresent is set to true and the key is already in the map an error will be thrown
	void addKeyAndString( const std::string& key, const std::string& str, bool throwIfAlreadyPresent = false);

	Config(); // please don't use this private constructor
public:
	boost::property_tree::ptree               m_propTree; 

    // Could write another constructor that initialized the data from a source other than an xml file.

	Config(const std::string& pathToXMLConfigFile);

	// if the key is in the map
	// then find(..) will return true and copy the string to the 'str' parameter
	// otherwise it will return false
	bool find(const std::string& key,        // input
		            std::string& str) const; // output

	// get(..) will throw an error if key is not found in the map
	std::string get(const std::string& key) const;

    BigNatural getRandomGeneratorSeed();

	std::string getDateFormat(); // throws on failure.
};

template<class T>
T pt_get(const boost::property_tree::ptree &pt, const std::string& key)
{   // The purpose of this function is to provide more diagnostic information than the standard pt.get<T>(.)
	boost::optional<T> t;
	if(!(t = pt.get_optional<T>(key)))
		QL_FAIL("pt_get(..): can't find key: '" << key << "' in property tree: " << std::endl
		        << toString(pt));
	return *t;
}

template<class T>
T pt_get_optional(const boost::property_tree::ptree &pt, const std::string& key, const T& defaultVal)
{
	boost::optional<T> t;
	if( t = pt.get_optional<T>(key)) // was able to obtain the object from the ptree
		return *t;
	else  // couldn't get the object from the ptree
		return defaultVal;
}

// pt_getDate(..) will throw if the date is not in the pTree with specified key.
Date pt_getDate(        const boost::property_tree::ptree&  pTree, 
				        const std::string&                  key); 

// pt_getDateOptional(..) return the defaultDate, when the pTree does not contain a date at 
// the specified key.
Date pt_getDateOptional(const boost::property_tree::ptree&  pTree, 
						const std::string&                  key, 
						const Date&                         defaultDate);

class MarketCaches; // forward declaration
class Contract;     // forward declaration
class Config;       // forward declaration
class ResultSet;    // forward declaration
class StockData;    // forward declaration

class CalculatorBase
{
protected:
	Contract*                     m_contract;
	MarketCaches*                 m_marketCaches;
	ResultSet*                    m_resultSet;

	boost::shared_ptr<StockData>  m_stockData; // not all calculators will have a stock data shared pointer.
public:
	CalculatorBase(Contract* pContract, MarketCaches* pMarketCaches, ResultSet* pResultSet);

	void setStockData(const std::string& ID, const std::string& IDType);
};

Frequency monthsInPeriodToFrequencyEnum(Integer monthsInPeriod);

// The following class is used for reporting diagnostics
// Possible extension: 1: could allow the output to somewhere other than std::cout
class Diagnostics
{
private:
    bool                                    m_initialized;  
	std::map<std::string, DiagnosticLevel>  m_mapOfSourceAndLevels;

	// message at the allowed level and above will be reported
	// those below the allowed level will be ignored.
	DiagnosticLevel                         m_defaultAllowedLevel;
public:
	Diagnostics();
	void initialize();
	void reportMessage(const std::string& msg, DiagnosticLevel requiredConfigLevel, const std::string& source="");
};

// A developer will call writeDianostics(..) and set the diagnostic requiredConfigLevel parameter.
// At run-time that requiredConfigLevel will be compared with the level specified in the config.
// If the config 'level' is on or above the parameter 'requiredConfigLevel', 
// then the message will be printed.
// otherwise it won't be printed.
// I.e. if the config level is 'mid' and the parameter requiredConfigLevel is 'high' 
// then the message will not be printed.
// Most often the level in the config will be 'low'. 
// It is recommended that the writeDiagnostics parameter 'requiredConfigLevel' be set at something above 'none'.
// That way when 'none' is requested in the config, no messages will be printed.
// We haven't made this a hard rule, because occasionally a programmer may wish to over-ride
// the 'none' in the config. 
// The 'source' is used so that in the config specific messages can be turned on.
void writeDiagnostics(const std::string&         msg            , 
					  DiagnosticLevel            requiredConfigLevel = high , // could be: none, low, mid, high, full
					  const std::string&         source  = "");

void getVectorOfDatesFromPTree(boost::property_tree::ptree&       pt,        // input
							   const std::string&                 outerKey,  // input 
							   const std::string&                 itemKey,   // input
							   std::vector<Date>&                 vDates);   // output

template<class T_key, class T_data>
void getVectorOfDatesFromBasicPTree(const boost::property_tree::basic_ptree<T_key, T_data>&  pt,        // input
							     const std::string&                                       itemKey,   // input
							     std::vector<Date>&                                       vDates)    // output
{
	boost::property_tree::basic_ptree<T_key, T_data>::const_iterator iter = pt.begin();

	while(iter != pt.end())
	{
		if(iter->first == itemKey)
		{
			writeDiagnostics("Found date in property tree: " + toString( iter->second.data()),
				             high, "getVectorOfDatesFromBasicPTree");
			vDates.push_back(stringToDate( iter->second.data(), getConfig()->getDateFormat()));
		}
		else
		{
			writeDiagnostics("In pTree, found: " + toString(iter->first.data()) + "\nwith: " + toString(iter->second.data()),
			                 high, "getVectorOfDatesFromBasicPTree");
		}

		iter++;
	}    
}

// Note: Jun-2010: have tested the following templated method with T=std::string. 
// It may need to be slightly reworked to function properly for other types.
template<class T>
void pt_getVector(const boost::property_tree::ptree &pt,                       // input
				  const std::string& outerTag, const std::string& innerTag,    // inputs
				  std::vector<T>& v)                                           // output
{
	boost::property_tree::basic_ptree<std::string, T>::const_iterator 
		iter = pt.get_child(outerTag).begin();
	while(iter != pt.get_child(outerTag).end())
	{
		if(iter->first == innerTag)  
			v.push_back(iter->second.data());

		iter++;
	}    
}

void addVectorOfHolidays(Calendar* pCal, const std::vector<Date>& holDates);

// We need one unambiguous owner of the Config shared pointer. That's where setGetConfig(.) 
// comes in. The function holds a static shared_ptr to the config.
// So we don't need any other class or function to hold a shared_ptr to the config
// The first call to setGetConfig(..) should set the config, there-after calls don't 
// need to set optional parameter pConfig.
// Will return NULL if the config can't be found.
Config* setGetConfig(boost::shared_ptr<Config> pConfig = boost::shared_ptr<Config>());

// getConfig(..) will throw a (nice, deliberate) exception if the config pointer is NULL.
Config* getConfig();

template<class T>
std::vector<T> subset(const std::vector<T>& vSource, Size start, Size numElms)
{
	std::vector<T> vOut(numElms);
	for(Size i = 0; i < numElms; i++)
		vOut[i] = vSource[start + i];

	return vOut;
}

class SettleRule
{
private:
	boost::shared_ptr<Calendar>  m_settleCal;
	boost::shared_ptr<Calendar>  m_obsCal;
	Integer                      m_settleLag; 
    BusinessDayConvention 		 m_settleConvention;

	SettleRule();
public:
    SettleRule(boost::shared_ptr<Calendar>  settleCal, 
			   boost::shared_ptr<Calendar>  obsCal,
			   Size                         settleLag,
			   BusinessDayConvention        settleConvention);

	Date getSettleDate     (Date observationDate);
	Date getObservationDate(Date settleDate);
};

void mergeHolCals(const Calendar* fromCal,             // input
	 			  const Date&     firstDateOfInterest,
				  const Date&     lastDateOfInterest,
				  Calendar*       toCal);              // output

boost::shared_ptr<Calendar> getHolCal(const std::vector<std::string>& vHolCalIDs, 
									  MarketCaches*                   pMarketCaches,
									  const Date&                     firstDateOfInterest, 
									  const Date&                     lastDateOfInterest);


#endif  // ifdef utilities.h
