#include "Utilities.hpp"
#include "MarketData.hpp"

std::string dayOfWeek(const Date& d)
{
	Weekday weekday = d.weekday();

	switch (weekday) 
	{
	     case Sun:  return "Sun";
		 case Mon:  return "Mon";
		 case Tue:  return "Tue";
		 case Wed:  return "Wed";
		 case Thu:  return "Thu";
		 case Fri:  return "Fri";
		 case Sat:  return "Sat";

		 default: QL_FAIL("dayOfWeek(.): Unknown weekday " << weekday);
	}
}

std::string toString(const Date d, const std::string& format)
{
    std::stringstream stream;

    if     ( format == "d-mmm-yyyy")      // for example: 7-May-2015
	   stream << d.dayOfMonth() << "-" << d.month() << "-" << d.year();	
	else if( format == "ddd d-mmm-yyyy")  // for example 'Mon 7-May-2015'
		stream << dayOfWeek(d) << " " << d.dayOfMonth() << "-" << d.month() << "-" << d.year();
	else if( format == "yyyy-mm-dd")      // ISO standard: for example: 2015-05-07 
	   stream << QuantLib::io::iso_date(d);
	else // could extend this function to deal with date formats other than d-mmm-yyyy
	{    // though if the number of required formats to be supported became very large,
		 // then we could parse the format string, rather than have one branch of the if statement 
		 // for each variation, but for now that looks like over-kill.
		QL_FAIL("toString(..): Unsupported format: " << format << ", could try d-mmm-yyyy or yyyy-mm-dd.");
	}
    return stream.str();
}

// Convert string to Month enum
Month monthAsStringToEnum(const std::string& monthAsString)
{  // We'll use the first 3 characters, so "March" will become "Mar"
   // Thus, here "MarXXXXXXXXXXXX" will be deemed to be a valid month string.
   // For now this IS case sensitive, i.e. "jan" is not a valid month, (though "Jan" is).

   std::string threeCharMonth = monthAsString.substr(0,3);
   Month monthAsEnum;

   if      ( threeCharMonth == "Jan" ) monthAsEnum = Jan;
   else if ( threeCharMonth == "Feb" ) monthAsEnum = Feb;
   else if ( threeCharMonth == "Mar" ) monthAsEnum = Mar;
   else if ( threeCharMonth == "Apr" ) monthAsEnum = Apr;
   else if ( threeCharMonth == "May" ) monthAsEnum = May;
   else if ( threeCharMonth == "Jun" ) monthAsEnum = Jun;
   else if ( threeCharMonth == "Jul" ) monthAsEnum = Jul;
   else if ( threeCharMonth == "Aug" ) monthAsEnum = Aug;
   else if ( threeCharMonth == "Sep" ) monthAsEnum = Sep;
   else if ( threeCharMonth == "Oct" ) monthAsEnum = Oct;
   else if ( threeCharMonth == "Nov" ) monthAsEnum = Nov;
   else if ( threeCharMonth == "Dec" ) monthAsEnum = Dec;
   else    { QL_FAIL("monthAsStringToEnum(..): Unrecognised month string: " << monthAsString); }

   return monthAsEnum;
}	   

// An alternative to this function is DateParser::parse(..) in dataparsers.cpp
// though it is not quite the same.
Date stringToDate(const std::string& dateAsString, const std::string& format)
{
    Integer dayOfMonth;
	Month   month;      // An enum that can take values 1 to 12 ( Jan to Dec)
	Integer year;

	if(    (format == "d-mmm-yyyy" )  // for example  7-May-2015
		|| (format == "dd-mmm-yyyy")) // for example 07-May-2015
	{
        // find position of first '-'
		size_t firstDash = dateAsString.find_first_of('-');

		// the first dash could either be in position 1 ( eg 7-May-2015) or position 2 ( eg 17-May-2015 )
		QL_REQUIRE((firstDash == 1)||( firstDash == 2), 
			        "stringToDate(..): Date format specified as d-mmm-yyyy ( eg 7-May-2015), but got date string: " + dateAsString);

		// Convert the string for day of month to an integer.
		dayOfMonth = atoi(dateAsString.substr(0, firstDash).c_str()); 

        size_t secondDash = dateAsString.find_first_of('-', firstDash+1);

        // Require the month sub-string to be exactly 3 characters long.
		QL_REQUIRE( (secondDash - firstDash) == 4,
			        "stringToDate(..): Date format specified as d-mmm-yyyy ( eg 7-May-2015), but got date string: " + dateAsString);
			        
        month = monthAsStringToEnum(dateAsString.substr(firstDash+1,3)); 

		// Require that there are 4 characters left for the year sub-string (otherwise will throw an error).
		QL_REQUIRE( (dateAsString.length() - secondDash) == 5, 
			        "stringToDate(..): Date format specified as d-mmm-yyyy ( eg 7-May-2015), but got date string: " + dateAsString);

		// Convert the string for year to an integer.
		year = atoi(dateAsString.substr(secondDash+1, 4).c_str()); 
	}
	else // could extend this function to deal with date formats other than d-mmm-yyyy formats
	{
		QL_FAIL("stringToDate(..): Unsupported format: " << format << ", could try d-mmm-yyyy.");
	}

	Date d(dayOfMonth, month, year);

	return d;
}

	// when throwIfAlreadyPresent is set to true and the key is already in the map an error will be thrown
void Config::addKeyAndString( const std::string& key, const std::string& str, bool throwIfAlreadyPresent)
{
	std::map<std::string, std::string>::iterator it;
    it = m_map.find(key);

	if( it == m_map.end()) // the key is NOT already in the map
	{   // adding a new entry into the map.
 		m_map.insert(std::pair<std::string, std::string>(key, str));       
	}
	else                   // the key is already in the map
	{
		QL_REQUIRE( !throwIfAlreadyPresent, "Config::addKeyAndString(..): Found that the key: " << key << " is already in the map.");
		it->second = str;  // over-writing the existing entry
	}
}

    // Could write another constructor that initialized the data from a source other than an xml file.

Config::Config(const std::string& XMLFilename)
{
	boost::property_tree::xml_parser::read_xml(XMLFilename, 
		                                       m_propTree, 
											   boost::property_tree::xml_parser::trim_whitespace);

	BOOST_FOREACH(boost::property_tree::ptree::value_type &v, m_propTree.get_child("config"))
	{
		if(v.first.data() != CONST_STR_xmlcomment)  // when the xml contains '<!-- ... -->' 
		{                                           // that will appear here tagged with <xmlcomment>
			addKeyAndString( v.first.data(), v.second.data(), true); 
		}
	}

	m_randomGeneratorSeedIsInitialized = false;
}

Config::Config() 
{ 
	QL_FAIL("Config::Config(): Don't allow the default constructor to be called."); 
}


// if the key is in the map
// then find(..) will return true and copy the string to the 'str' parameter
// otherwise it will return false
bool Config::find(const std::string& key, // input
	              std::string& str) const // output
{
	std::map<std::string, std::string>::const_iterator it = m_map.find( key);

	if( it != m_map.end())                // i.e. the key is in the map
	{
        str = it->second;
		return true;
	}
	else                                  // The key is not in the map.
	    return false;
}

// get(..) will throw an error if key is not found in the map
std::string Config::get(const std::string& key) const
{
	std::string str;
	QL_REQUIRE(find(key, str), "Config::get(..): Can't find the key: " << key << " in the Config ");
	return str;
}

std::string Config::getDateFormat()        // throws on failure.
{
	if(!m_dateFormat.length())             // m_dateFormat hasn't yet been initialized.
		m_dateFormat = get("date_format"); // this may throw

	return m_dateFormat;
}

BigNatural Config::getRandomGeneratorSeed() 
{
	if(!m_randomGeneratorSeedIsInitialized)
	{
		std::string seedAsString;
        if(find("random_generator_seed", seedAsString))
            m_randomGeneratorSeed = atoi(seedAsString.c_str());
		else 
		{
			m_randomGeneratorSeed = CONST_defaultRandomGeneratorSeed;
			writeDiagnostics(  "Didn't find random_generator_seed in config so using default: "
				             + toString(CONST_defaultRandomGeneratorSeed), 
							   mid, "Config::getRandomGeneratorSeed"); 
		}
	    m_randomGeneratorSeedIsInitialized = true;
	}

	return m_randomGeneratorSeed;
}

std::string ptToString(const boost::property_tree::ptree& pt, const std::string indent)
{
	std::stringstream stream;
	boost::property_tree::ptree::const_iterator iter = pt.begin();
			
	while( iter != pt.end() ) 
	{
		stream << indent << "<" << iter->first.data() << ">";

		std::string data = iter->second.data();
		if(data.size() == 0) // have a sub-tree, so call this function recursively.
			stream << std::endl << ptToString(iter->second, indent + "  ") << indent;
		else                 // have some simple data
			stream << iter->second.data(); 

		stream << "</" << iter->first.data() << ">" << std::endl;

		iter++;
	}
	return stream.str();
}

std::string toString(const boost::property_tree::ptree& pt)
{
	return ptToString(pt, "");
}

CalculatorBase::CalculatorBase(Contract* pContract, MarketCaches* pMarketCaches, ResultSet* pResultSet)
{
	QL_REQUIRE(pContract != NULL, 
		       "CalculatorBase::CalculatorBase(..): Have been passed a NULL pointer as a contract.\n"
		       << "This may have been caused by a failed dynamic_cast.");

	m_contract      = pContract;
	m_marketCaches  = pMarketCaches;
	m_resultSet     = pResultSet;
}

void CalculatorBase::setStockData(const std::string& ID, const std::string& IDType)
{
	m_stockData = m_marketCaches->getStockDataCache()->get(ID, IDType);
}

Frequency monthsInPeriodToFrequencyEnum(Integer monthsInPeriod)
{
	switch (monthsInPeriod) 
	{
		 case 1:            return Monthly;
		 case 2:            return Bimonthly;
		 case 3:            return Quarterly;
		 case 4:            return EveryFourthMonth;
		 case 6:            return Semiannual;
		 case 12:           return Annual;

		 default: QL_FAIL("monthsInPeriodToFrequencyEnum(.): Can't deal with: " 
					      << monthsInPeriod 
						  << ". Should be a factor of 12, i.e. : 1, 2, 3, 4, 6 or 12.");
	}
}

std::string toString(ContractCategory e)
{
	switch (e) 
	{
		 case barrier_option:         return "barrier_option";
		 case call_spread_cpn_note:   return "call_spread_cpn_note";
		 case convertible_bond:       return "convertible_bond";
		 case equity_linked_note:     return "equityLinkedNote";
		 case koda:                   return "koda";
		 case range_accrual:          return "range_accrual";
         case vanilla_option:         return "vanilla_option";

		 default: QL_FAIL("toString(.): Unrecognised ContractCategory: " << e);
	}
}

ContractCategory ContractCategoryStringToEnum(const std::string& str)
{   // If the list of categories grows large could improve the efficiency by using map.
	// or some other solution that did a binary search
	     if ( str == "accumulator")          return koda; // we keep this for backwards compatibility
	else if ( str == "barrier_option")       return barrier_option;
	else if ( str == "call_spread_cpn_note") return call_spread_cpn_note;
	else if ( str == "convertible_bond")     return convertible_bond;
	else if ( str == "equity_linked_note")   return equity_linked_note;
	else if ( str == "koda")                 return koda;
	else if ( str == "range_accrual")        return range_accrual;
	else if ( str == "vanilla_option" )      return vanilla_option;
	else QL_FAIL("ContractCategoryToEnum(.): Did not recognise the contract category: " << str);
}
// pt_getDate(..) will throw if the date is not in the pTree with specified key.
Date pt_getDate(        const boost::property_tree::ptree&  pTree, 
				        const std::string&                  key)
{
	std::string dateAsString = pt_get<std::string>(pTree, key);
	return stringToDate( dateAsString, getConfig()->getDateFormat());
}

// pt_getDateOptional(..) return the defaultDate, when the pTree does not contain a date at 
// the specified key.
Date pt_getDateOptional(const boost::property_tree::ptree&  pTree, 
						const std::string&                  key, 
						const Date&                         defaultDate)
{
	boost::optional<std::string> optDateStr;
	if( optDateStr = pTree.get_optional<std::string>(key)) // was able to obtain the object from the ptree
		return stringToDate( *optDateStr, getConfig()->getDateFormat());
	else  // couldn't get the object from the ptree
		return defaultDate;
}

Diagnostics::Diagnostics()                   
{	 
    m_defaultAllowedLevel = low;  // by default few diagnostic messages are reported.
	m_initialized         = false;
	initialize();
}

void Diagnostics::initialize() 
{
    Config* pConfig;      //        setGetConfig() can return a NULL, so we test for a NULL,
	if(!m_initialized && (pConfig = setGetConfig())) // deliberately use assignment '=',
	{                                                     // rather than comparison '=='
		m_initialized = true;

		// Setting m_mapOfKeysAndLevels
		boost::optional<std::string> levelAsStr = pConfig->m_propTree.get_optional<std::string>("config.diagnostics.default_level");
		m_defaultAllowedLevel = (levelAsStr ? diagnosticLevelStringToEnum(*levelAsStr) 
											: CONST_defaultDiagnosticLevel);

		boost::property_tree::ptree pt = pConfig->m_propTree.get_child("config.diagnostics");
		boost::property_tree::ptree::const_iterator iter = pt.begin();

		while( iter != pt.end() ) 
		{
			if(!strcmp(iter->first.data(), "setting"))  
			{   
				DiagnosticLevel level = diagnosticLevelStringToEnum(pt_get<std::string>(iter->second, "allowed_level"));
				std::string source = pt_get<std::string>(iter->second, "source");
				m_mapOfSourceAndLevels.insert(std::pair<std::string, DiagnosticLevel>(source, level));
			}
			iter++;
		}
	}
}

void Diagnostics::reportMessage(const std::string&  msg,
								DiagnosticLevel     requiredConfigLevel,  
								const std::string&  source)
{  initialize(); // will do nothing if it has already been initialized
   std::map<std::string, DiagnosticLevel>::iterator iter;
   iter = m_mapOfSourceAndLevels.find(source);
   if( iter != m_mapOfSourceAndLevels.end()) // found a level for the source
   {
      if( iter->second >= requiredConfigLevel)             // the level in config is high enough
		  std::cout << source << ": " << msg << std::endl;
      // else do nothing, i.e. don't report the message
   }
   else if(m_defaultAllowedLevel >= requiredConfigLevel) // (the key has not been found) && (level in config is high enough)
   {
	   std::cout << source << ": " << msg << std::endl;
   }
   // else do nothing, i.e. don't report the message
}

DiagnosticLevel diagnosticLevelStringToEnum(const std::string& str)
{
	     if(str == "none"  ) return none;
	else if(str == "low"   ) return low;
	else if(str == "mid"   ) return mid;
	else if(str == "high"  ) return high;
	else if(str == "full"  ) return full;
	else   
	{
		QL_FAIL("diagnosticLevelStringToEnum(..): Unrecognised diagnostic level string "
			    << str << ". Could try 'none', 'low', 'mid', 'high' or 'full'.");
	}
}

// A developer will call writeDianostics(..) and set the diagnostic level parameter.
// At run-time that level will be compared with the level specified in the config.
// If the config 'level' is on or above the parameter 'level', 
// then the message will be printed.
// otherwise it won't be printed.
// I.e. if the config level is mid and the parameter level is 'high' then the message will not be printed.
// Most often the level in the config will be 'none'. 
// It is recommended that the writeDiagnostics parameter 'level' be set at something above 'none'.
// That way when 'none' is requested in the config, no messages will be printed.
// We haven't made this a hard rule, because occasionally a programmer may wish to over-ride
// the 'none' in the config. 
// The 'source' is used so that in the config specific messages can be turned on.
void writeDiagnostics(const std::string& msg, DiagnosticLevel requiredConfigLevel, const std::string& source)
{
	static Diagnostics diagnostics; // we have a static member here, since we only want it initialized once.
    diagnostics.reportMessage(msg, requiredConfigLevel, source);
}


void getVectorOfDatesFromPTree(boost::property_tree::ptree&       pt,        // input
							   const std::string&                 outerKey,  // input 
							   const std::string&                 itemKey,   // input
							   std::vector<Date>&                 vDates)    // output
{
	BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child(outerKey))
	{
		if(v.first.data() == itemKey)  
			vDates.push_back(stringToDate( v.second.data(), getConfig()->getDateFormat()));
	}    
}

// We need one unambiguous owner of the Config shared pointer. That's where setGetConfig(.) 
// comes in. The function holds a static shared_ptr to the config.
// So we don't need any other class or function to hold a shared_ptr to the config
// The first call to setGetConfig(..) should set the config, there-after calls don't 
// need to set optional parameter pConfig.
// Will return NULL if the config can't be found.
Config* setGetConfig(boost::shared_ptr<Config> pConfig) // optional parameter
{   
	static boost::shared_ptr<Config> staticConfig;
    if(pConfig)
		staticConfig = pConfig;

	return (staticConfig ? &(*staticConfig) : NULL);
}

// getConfig(..) will throw a (nice, deliberate) exception if the config pointer is NULL.
Config* getConfig()
{   
	Config* pConfig = setGetConfig();
	QL_REQUIRE(pConfig, "getConfig(.): can't return config pointer because it hasn't been set");
	return &(*pConfig);
}

void addVectorOfHolidays(Calendar* pCal, const std::vector<Date>& holDates)
{
	for(Size i = 0; i < holDates.size(); i++)
	{
		pCal->addHoliday(holDates[i]);
	}
}

SettleRule::SettleRule(boost::shared_ptr<Calendar>  settleCal, 
					   boost::shared_ptr<Calendar>  obsCal,
					   Size                         settleLag,
					   BusinessDayConvention        settleConvention)
{
	m_settleCal        = settleCal;
	m_obsCal           = obsCal;
	m_settleLag        = settleLag;
	m_settleConvention = settleConvention;

	if( settleLag < 0)
	{
		writeDiagnostics("Warning: using negative settlement lag: " + toString(settleLag),
	           	         low, "SettleRule::SettleRule");
	}
}

SettleRule::SettleRule()
{ 
	QL_FAIL("SettleRule(): Please don't use this constructor."); 
}

Date SettleRule::getSettleDate(Date observationDate)
{   // PK TODO: please test over weekend!
	return m_settleCal->advance( observationDate, m_settleLag, Days, m_settleConvention);
}

Date SettleRule::getObservationDate(Date settleDate)
{   // we first roll back using the settleCal, then adjust using the obs cal.
	Date rolled = m_settleCal->advance( settleDate, - m_settleLag, Days, Preceding);
	return        m_obsCal->   advance( rolled,                 0, Days, Preceding);
}

void mergeHolCals(const Calendar* fromCal,             // input
	 			  const Date&     firstDateOfInterest,
				  const Date&     lastDateOfInterest,
				  Calendar*       toCal)               // output
{
    std::vector<Date> dates = Calendar::holidayList( *fromCal,
		                                             firstDateOfInterest, 
		                                             lastDateOfInterest, 
												     false);
	writeDiagnostics("Between " + toString(firstDateOfInterest) + " and " 
		             + toString(lastDateOfInterest) + " found " + toString(dates.size())
					 + " dates.", high, "mergeHolCals");
	for(Size i = 0; i < dates.size(); i++)
	{
	    toCal->addHoliday(dates[i]);
		writeDiagnostics("Just added date: " + toString(dates[i], "ddd d-mmm-yyyy"),
			             high, "mergeHolCals");
	}
}

boost::shared_ptr<Calendar> getHolCal(const std::vector<std::string>& vHolCalIDs, 
									  MarketCaches*                   pMarketCaches,
									  const Date&                     firstDateOfInterest, 
									  const Date&                     lastDateOfInterest)
{
	boost::shared_ptr<Calendar> mainCal;

	if(  vHolCalIDs.size() > 0)
	{
		Size calNumber = 0;
		mainCal = pMarketCaches->getCalendarCache()->get(vHolCalIDs[0]);

		boost::shared_ptr<Calendar> currentCal;
		for(calNumber = 1 ; calNumber < vHolCalIDs.size(); calNumber++)
		{
			currentCal = pMarketCaches->getCalendarCache()->get(vHolCalIDs[calNumber]);
			mergeHolCals(&(*currentCal), firstDateOfInterest, lastDateOfInterest, &(*mainCal)); 
		}

		writeDiagnostics("Just added " + toString(vHolCalIDs.size()) + " holiday calendars. ",
		                 high, "getHolCal");
	}
	else // when no holiday calendars are set will use just a weekend only cal
	{
		boost::shared_ptr<BespokeCalendar> bCal = (boost::shared_ptr<BespokeCalendar>) new BespokeCalendar("Weekdays");
		bCal->addWeekend(Saturday);
		bCal->addWeekend(Sunday);
		mainCal = (boost::shared_ptr<Calendar>) bCal;
		writeDiagnostics("Found no holidays specified, so using the 5 weekdays as business days", low, "getHolCal");
	}

	return mainCal;
}
