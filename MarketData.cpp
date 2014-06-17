#include "MarketData.hpp"


//////////////////////////////////////////////////////////////////////////////
// Calendar
CalendarXMLSource::CalendarXMLSource(MarketCaches* marketCaches)
		: 	MarketObjXMLSource<boost::shared_ptr<Calendar> >              // calling the constructor of
		    (marketCaches, "calendars_xml_filename", "market_data.calendars")  // the base class
{
    m_usingSingleKey = true; // the default is false, which means using dual key.
}

//return type: boost::shared_ptr<CacheSingleKey<boost::shared_ptr<Calendar> > > 
MarketCaches::CalendarCacheSharedPointer  MarketCaches::getCalendarCache()
{
	return getCache<boost::shared_ptr<Calendar>, CalendarXMLSource, CacheSingleKey<boost::shared_ptr<Calendar> > >
		             ("calendar_source", // the key used in the config to specify the data source 
	                  &m_calendarCache); // this will be set
}

//return type: boost::shared_ptr<CacheDualKey<boost::shared_ptr<Prices> > > 
MarketCaches::StockPricesCacheSharedPointer  MarketCaches::getStockPricesCache()
{
	return getCache<boost::shared_ptr<Prices>, StockPricesXMLSource, CacheDualKey<boost::shared_ptr<Prices> > >
		             ("prices_source",      // the key used in the config to specify the data source 
	                  &m_stockPricesCache); // this will be set
}

//return type: boost::shared_ptr<CacheDualKey<boost::shared_ptr<Prices> > > 
MarketCaches::FXPricesCacheSharedPointer  MarketCaches::getFXPricesCache()
{
	return getCache<boost::shared_ptr<Prices>, FXPricesXMLSource, CacheDualKey<boost::shared_ptr<Prices> > >
		             ("prices_source",      // the key used in the config to specify the data source 
	                  &m_FXPricesCache); // this will be set
}

// get(..) will throw an error if the Calendar is not found.
boost::shared_ptr<Calendar> CalendarXMLSource::get_s(const std::string& calID)// , const std::string& ignoredParameter)
{    
	boost::shared_ptr<Calendar> cal;

	boost::property_tree::ptree::const_iterator iter = m_PropTree.begin();			
	bool found = false;
	while( ( iter != m_PropTree.end() ) && !found)
	{   
		if(CONST_STR_calendar == iter->first.data() )
		{   
			std::string cal_IDinXML = iter->second.get<std::string>("id");
            
			if( cal_IDinXML == calID)
			{   
				boost::shared_ptr<BespokeCalendar> bCal = (boost::shared_ptr<BespokeCalendar>) new BespokeCalendar(calID);
				bCal->addWeekend(Saturday);
				bCal->addWeekend(Sunday);

				std::vector<Date> holDates;
                getVectorOfDatesFromBasicPTree(iter->second , "holiday", holDates); 
				addVectorOfHolidays(&(*bCal), holDates);

				cal = (boost::shared_ptr<Calendar>) bCal;

				found  = true;
			}
		}
		iter++; // keep searching
	}			// end of while(..)
	QL_REQUIRE(found, "Was unable to find the holiday calendar " << calID
		              << " in the xml market data.");
	return cal;
}               // end of method          

////////////////////////////////////////////////////////////////////////////////////////
FXVolXMLSource::FXVolXMLSource(MarketCaches* marketCaches)
		: 	MarketObjXMLSource<boost::shared_ptr<BlackVolTermStructure> >      
		    (marketCaches, "fx_vol_data_xml_filename", "market_data.fx_vols")  
{}

YieldTS_XMLSource::YieldTS_XMLSource(MarketCaches* marketCaches)
	: 	MarketObjXMLSource<boost::shared_ptr<YieldTermStructure> >
	    (marketCaches, "risk_free_rate_xml_filename", "market_data.risk_free_rates")
{}

StockData::StockData()                         
{ 
		m_dividendYield     =  0.0;
		m_repoRate          =  0.0; // positive repo rates cause the forwards to decrease
		m_spotPrice         = -1.0; // an invalid spot price
		m_flatVol           = -1.0; // an invalid vol
		m_creditSpread      =  0.0;
		m_creditSpreadIsSet = false;
} 

///////////////////////////////////////////////////////////////////////
// get methods
std::string  StockData::getID()                { return m_ID;                }
std::string  StockData::getIDType()            { return m_IDType;            }
std::string  StockData::getName()              { return m_name;              }
std::string  StockData::getCurrency()          { return m_currency;          }
Real         StockData::getDividendYield()     { return m_dividendYield;     }
Real         StockData::getRepoRate()          { return m_repoRate;          }
Real         StockData::getCreditSpread()      
{ 
	QL_REQUIRE( m_creditSpreadIsSet, "StockData::getCreditSpread(): the credit spread has not been set for stock: "
		                             << getID());
	return m_creditSpread;
}

std::string  StockData::getHolidayCalendarID() 
{
	if(m_holidayCalendarID.length() == 0)
		writeDiagnostics("Warning: Have an empty string as the holiday calendar ID, with stock ID: " + m_ID, 
		                 low, "StockData::getHolidayCalendarID"); 

	return m_holidayCalendarID; 
}

Real StockData::getFlatVol()              
{ 
	QL_REQUIRE( m_flatVol > 0.0, "StockData::getFlatVol(): " << m_ID 
			<< " The flat vol (" << m_flatVol << ") has not yet been set to a valid vol, i.e. > 0.");

	return m_flatVol;     
}
///////////////////////////////////////////////////////////////////////
// class Prices
Prices::Prices(Date evalDate, const std::string& ID1, const std::string& ID2) 
{ 	
	m_evalDate                  = evalDate;
	m_currentPriceIsSet         = false;  
	m_ID1                       = ID1;
	m_ID2                       = ID2;
}

void Prices::addPrice(const Date&  date,    Real price)
{
	m_mapOfPrices.insert(std::pair<Date, Real>(date, price));
}

void Prices::setCurrentPrice(Real currentPrice)
{
	m_currentPriceIsSet = true;
	m_currentPrice      = currentPrice;
	m_mapOfPrices.insert(std::pair<Date, Real>(m_evalDate, currentPrice));
}

bool Prices::findPrice(const Date&  date,         // input         returns false if the price is absent
		               Real&        price)  const // output
{
	QL_REQUIRE(date <= m_evalDate, "Prices::findPrice(..): Can only get prices when \ndate ("
		                          << date << ") is on or before eval date (" << m_evalDate << ").");
	std::map<Date, Real>::const_iterator iter = m_mapOfPrices.find( date );
	if(iter != m_mapOfPrices.end()) // found the date in the map
	{
		price = iter->second;
		return true;
	}
	else
		return false;
}

Real Prices::getPrice(const Date&  date) const  // will throw and exception when price is not found
{
	Real price;
	QL_REQUIRE(findPrice(date, price), "Prices::getPrice(.): was unable to find price for date "
		                                << date << ( (m_ID1.length()+m_ID2.length()) ? ", for " : "") 
										<< m_ID1 << ", " << m_ID2);
	return price;
}

Real Prices::getCurrentPrice() const //  may throw
{
	if( m_currentPriceIsSet)
		return m_currentPrice;
	else
	{
		writeDiagnostics(  "Current price not explicitly set,\nso will use price on eval date: " 
			             + toString(m_evalDate), low, "Prices::getCurrentPrice");
			
		return getPrice(m_evalDate);
	}
}


///////////////////////////////////////////////////////////////////////
// set methods
void StockData::setID                (const std::string& ID          ) { m_ID                 = ID;           }
void StockData::setIDType            (const std::string& IDType      ) { m_IDType             = IDType;       }
void StockData::setName              (const std::string& name        ) { m_name               = name;         }
void StockData::setHolidayCalendarID (const std::string& cal         ) { m_holidayCalendarID  = cal;          }
void StockData::setCurrency          (const std::string& currency    ) { m_currency           = currency;     }
void StockData::setFlatVol           (Real               flatVol     ) { m_flatVol            = flatVol;      }
void StockData::setSpotPrice         (Real               spotPrice   ) { m_spotPrice          = spotPrice;    }
void StockData::setDividendYield     (Real               divYield    ) { m_dividendYield      = divYield;     }
void StockData::setRepoRate          (Real               repoRate    ) { m_repoRate           = repoRate;     }
void StockData::setCreditSpread      (Real               creditSpread) 
{ 
	m_creditSpread       = creditSpread;
    m_creditSpreadIsSet  = true;
}

StockDataXMLSource::StockDataXMLSource(MarketCaches* marketCaches)
		: 	MarketObjXMLSource<boost::shared_ptr<StockData> >
		    (marketCaches, 
			"stock_data_xml_filename", // tag in config xml used to specify the path to xml data 
			"market_data.stock_data")  // tag in the market data
	{}

StockPricesXMLSource::StockPricesXMLSource(MarketCaches* marketCaches)
		: 	MarketObjXMLSource<boost::shared_ptr<Prices> >
		    (marketCaches,
			"prices_xml_path",     // tag in config xml used to specify the path to xml data 
			"market_data.prices")  // tag in the market data
	{}

FXPricesXMLSource::FXPricesXMLSource(MarketCaches* marketCaches)
		: 	MarketObjXMLSource<boost::shared_ptr<Prices> >
		    (marketCaches,
			"prices_xml_path",       // tag in config xml used to specify the path to xml data 
			"market_data.fx_spots")  // tag in the market data
	{}

MarketCaches::MarketCaches()  // Constructor.
{   // The caches will be initialized only when they are requested.

    m_XMLPath = getConfig()->get("market_data_xml_path");
    initialize();
}

void MarketCaches::initialize()
{
	// Potentially the eval_date could be moved from the config to the market data source.
	std::string evalDateStr = getConfig()->get("eval_date");
    if( !strcmp(evalDateStr.c_str(), "today"))
		m_evalDate = Date::todaysDate();
	else
		m_evalDate = stringToDate(evalDateStr, getConfig()->getDateFormat());
}

MarketCaches::MarketCaches(const std::string& pathToXMLMarketData)
{
	m_XMLPath = pathToXMLMarketData;
	initialize();
}

//return type: boost::shared_ptr<CacheDualKey<boost::shared_ptr<YieldTermStructure> > > 
MarketCaches::YieldTSCacheSmartPointer MarketCaches::getYieldTSCache()
{
	return getCache<boost::shared_ptr <YieldTermStructure>, YieldTS_XMLSource, CacheDualKey<boost::shared_ptr<YieldTermStructure> > >
	              ("yield_term_structure", // the key used in the config to specify the data source 
				  &m_YieldTSCache);        // this will be set
}

/////////////////////////////////////////////////////////////////////////////
MarketCaches::FXVolCacheSmartPointer  MarketCaches::getFXVolCache()
{
	return getCache<boost::shared_ptr<BlackVolTermStructure>, FXVolXMLSource, CacheDualKey<boost::shared_ptr<BlackVolTermStructure> > >
		           ("fx_vol_source",                // the key used in the config to specify the data source 
	               &m_FXVolCache);                  // this will be set
}

// The following method will only be used when an XML market data source is used.
boost::property_tree::ptree* MarketCaches::getXMLPropTree()
{
	if ( m_XMLPropTree.empty()) 
	{   // clearly we just want to load and parse the xml market data filename once
		// potentially it is very large.
	    boost::property_tree::xml_parser::read_xml(m_XMLPath, 
		                                           m_XMLPropTree, 
											       boost::property_tree::xml_parser::trim_whitespace);
	}
	// this class (MarketCaches) will own the property tree. This method will return a pointer to it.
	return &m_XMLPropTree;
}

Date MarketCaches::getEvalDate()   
{ 
	return m_evalDate; 
}

MarketCaches::StockDataCacheSharedPointer  MarketCaches::getStockDataCache()
{
	return getCache<boost::shared_ptr<StockData>, StockDataXMLSource, CacheDualKey<boost::shared_ptr<StockData> > >
		           ("stock_data",                 // the key used in the config to specify the data source 
				   &m_stockDataCache);
}

// get(..) will throw an error if the FXVol is not found.
boost::shared_ptr<BlackVolTermStructure> FXVolXMLSource::get(const std::string& currency1, 
															 const std::string& currency2)
{    
	QL_REQUIRE( currency1.size() > 0,  
		        "FXVolXMLSource::get(..): currency1 identifier can't be empty string.");

	QL_REQUIRE( currency2.size() > 0,  
		        "FXVolXMLSource::get(..): currency2 identifier can't be empty string.");

	QL_REQUIRE( currency1 != currency2, "FXVolXMLSource::get(..): There's no FX vol between currency: '"
		                                << currency1 << "' and itself.");

	boost::shared_ptr<BlackVolTermStructure> volObj;
	bool found = false;

	using boost::property_tree::ptree;
    ptree::const_iterator iter = m_PropTree.begin();
			
	while( ( iter != m_PropTree.end() ) && !found)
	{
		if( CONST_STR_fx_vol == iter->first.data() )
		{   
			std::string dataCcy1 = iter->second.get<std::string>("ccy1");
			std::string dataCcy2 = iter->second.get<std::string>("ccy2");

			if(   ( (dataCcy1 == currency1) && (dataCcy2 == currency2) )
			   || ( (dataCcy1 == currency2) && (dataCcy2 == currency1) ) )
			{
                 Real FXVol = iter->second.get<Real>("vol");
			     QL_REQUIRE(FXVol > 0, "FXVolXMLSource::get(..): Found FX vol for " 
					                    << dataCcy1 << " " << dataCcy2  << " to be " << FXVol 
									    << ", but it should be strictly greater than zero."); 

				 volObj = (boost::shared_ptr<BlackVolTermStructure>)
					             new BlackConstantVol(m_marketCaches->getEvalDate(), 
				                                      TARGET(),       // for now will use the target calendar 
				                                      FXVol,
                                                      Actual365Fixed() );
				 found  = true;
			}
		}		// end of if( tag == v.first.data() ) ...
		iter++; // keep searching
	}			// end of while(..)
	QL_REQUIRE(found, "Was unable to find volatility in the xml market data for: " 
		              << currency1 << "-" << currency2);
	return volObj;
}               // end of method          

// get(..) will throw an error if the Yield Term Structure is not found.
boost::shared_ptr<YieldTermStructure> YieldTS_XMLSource::get(const std::string& yieldType, 
															 const std::string& currency)
{
	writeDiagnostics("Seeking YieldTS from XML source with yieldType: " + yieldType
		             + "\nand currency: " + currency, high, "YieldTS_XMLSource::get");
	// For now this method will require that yieldType is set to "risk_free"
	QL_REQUIRE( yieldType == CONST_STR_risk_free_rate, "YieldTS_XMLSource::get(..): For now yieldType must be '"
		        << CONST_STR_risk_free_rate << "' here it is: " << yieldType << "." << std::endl);

	QL_REQUIRE( currency.size() > 0,  
		        "YieldTS_XMLSource::get(..): currency identifier can't be empty string.");
    
	boost::shared_ptr<YieldTermStructure> yieldTS;
	bool found = false;

	using boost::property_tree::ptree;
    ptree::const_iterator iter = m_PropTree.begin();
			
	while( ( iter != m_PropTree.end() ) && !found)
	{
		if( iter->first.data() == CONST_STR_risk_free_rate ) // i.e. found the 'risk_free_rate' tag
		{   
			std::string dataCcy = iter->second.get<std::string>("currency");

			if(dataCcy == currency)
			{
                 Real flatRate = iter->second.get<Real>("flat_rate");
			     QL_REQUIRE(flatRate >= 0, "YieldTS_XMLSource::get(..): Found flat_rate for " 
					                       << currency << " to be " << flatRate * 100.0
									       << "%, but it should be greater than or equal to zero."); 

			     QL_REQUIRE(flatRate < 1,  "YieldTS_XMLSource::get(..): Found flat_rate for " 
					                       << currency << " to be " << flatRate * 100.0 
									       << "%, but it should be less than 100%."); 

                 yieldTS = (boost::shared_ptr<YieldTermStructure>) 
					       new FlatForward(m_marketCaches->getEvalDate(), flatRate, Actual365Fixed() );

				 writeDiagnostics("Found YieldTS in xml source, with flat rate: " + toString(flatRate),
					              high, "YieldTS_XMLSource::get");
				 found  = true;
			}
		}		// end of if( CONST_STR_risk_free_rate == iter->first.data() ) ...
		iter++; // keep searching
	}			// end of while(..)
	QL_REQUIRE(found, "YieldTS_XMLSource::get(..): Was unable to find risk_free_rate in the xml market data for: " 
		              << currency);
	return yieldTS;
}               // end of method          

boost::shared_ptr<Prices> StockPricesXMLSource::get(const std::string& ID, const std::string& IDType)
{
	QL_REQUIRE(ID.length(), "StockPricesXMLSource::get(..): can't get the price with ID an empty string");

	boost::shared_ptr<Prices> prices;
	bool found = false;

	using boost::property_tree::ptree;
    ptree::const_iterator iter = m_PropTree.begin();
			
	while( ( iter != m_PropTree.end() ) && !found)
	{
		if( iter->first.data() == CONST_STR_item)
		{   
			std::string IDinData      = iter->second.get<std::string>("id");
			
			boost::optional<std::string> ID_typeInData;

			bool matchingIDTypes = true; 
			// We have two values for ID type, one is passed into this method (IDType), 
			// the other (IDTypeInData) is obtained from the xml data. 
			// If either is an empyt string then matchingIDTypes is set to true
			// otherwise mathcingIDTypes = ( IDType == IDTypeInData)
			
			if( ID_typeInData = iter->second.get_optional<std::string>("id_type"))
			{   // have been able to retrieve ID_type from xml data 
				if(IDType.size() != 0)
					matchingIDTypes = ( (*ID_typeInData) == IDType);
			}   // otherwise leave matchingIDTypes at true.
		
			// matchingIDTypes(..) will return true if either string is empty.
			if((IDinData == ID) && matchingIDTypes)
			{
				prices = (boost::shared_ptr<Prices>) new Prices(m_marketCaches->getEvalDate(), ID);

				boost::optional<Real> currentPrice;
				if( currentPrice = iter->second.get_optional<Real>("current_price")) // was able to obtain the object from the ptree
					prices->setCurrentPrice(*currentPrice);

				boost::property_tree::basic_ptree<std::string, std::string>::const_iterator innerIterator = iter->second.begin();
				while( innerIterator != iter->second.end())
				{
					if(!strcmp(innerIterator->first.c_str(),"price"))
					{
						Date date  = pt_getDateOptional   (innerIterator->second, "date",  Date());
						Real price = pt_get_optional<Real>(innerIterator->second, "value", -99999.0);

						if((date != Date()) && (price != -99999.0))
						{
							prices->addPrice(date, price);
							writeDiagnostics("For " + ID + ", added date " + toString(date) + " with price " + toString(price),
							                 high, "StockPricesXMLSource::get");
						}
						else
						{
							std::string msg =  "For " + ID + " was unable to get a valid price and date from the node:\n"
							                 + toString(innerIterator->second); 
							writeDiagnostics(msg, low, "StockPricesXMLSource::get");
						}
					}
 
					innerIterator++;
				}

				found = true;
			}
			iter++;
		}
	}
	QL_REQUIRE(found, "StockPricesXMLSource::get(..):\n"
		<< "Was unable to find the stock prices for: "  
		<< (IDType.length() > 0 ? toString("") : IDType + toString(": "))
		<< ID);

	return prices;
}

// The value of 1 unit of ID1 quoted in currency ID2
boost::shared_ptr<Prices> FXPricesXMLSource::get(const std::string& ID1, const std::string& ID2)
{
	QL_REQUIRE( ID1.length() == 3, "FXPricesXMLSource::get(..): FX ID1 must be 3 letters here it is: " << ID1 << ".");
	QL_REQUIRE( ID2.length() == 3, "FXPricesXMLSource::get(..): FX ID2 must be 3 letters here it is: " << ID2 << ".");

	boost::shared_ptr<Prices> prices;
	bool found = false;
	boost::property_tree::ptree::const_iterator iter = m_PropTree.begin();
			
	while( ( iter != m_PropTree.end() ) && !found)
	{
		if( iter->first.data() == CONST_STR_item)
		{   
			std::string ID1inData      = pt_get<std::string>( iter->second, "id1");
			std::string ID2inData      = pt_get<std::string>( iter->second, "id2");

			if(    ((ID1inData == ID1) && (ID2inData == ID2))
				|| ((ID1inData == ID2) && (ID2inData == ID1)))
			{  
  		        bool isReciprocal = ((ID1inData == ID2) && (ID2inData == ID1));
				prices = (boost::shared_ptr<Prices>) new Prices(m_marketCaches->getEvalDate(), ID1, ID2);

				Real currentPrice = pt_get_optional<Real>(iter->second, "current_price", 0.0); 
				if( currentPrice > 0.0)
					prices->setCurrentPrice(isReciprocal ? 1.0/currentPrice : currentPrice);

				boost::property_tree::basic_ptree<std::string, std::string>::const_iterator 
					innerIterator = iter->second.begin();
				while( innerIterator != iter->second.end())
				{
					if(!strcmp(innerIterator->first.c_str(),"price"))
					{
						Date date  = pt_getDateOptional   (innerIterator->second, "date",  Date());
						Real price = pt_get_optional<Real>(innerIterator->second, "value", 0.0);
						
						if((date != Date()) && (price != 0.0))
						{
							if( isReciprocal ) 
								price = 1.0 / price;

							writeDiagnostics("For " + ID1 + "-" + ID2 + ", added date " + toString(date) 
								             + " with price " + toString(price),
							                 high, "StockPricesXMLSource::get");

							prices->addPrice(date, price);

						}
						else
						{
							std::string msg =  "For " + ID1 + "-" + ID2 
								             + " was unable to get a valid price and date from the node:\n"
							                 + toString(innerIterator->second); 
							writeDiagnostics(msg, low, "StockPricesXMLSource::get");
						}
					}
					innerIterator++;
				}
				found = true;
			}
		}                // end of 'if( iter->first.data() == CONST_STR_item)'
		iter++;
	}                    // end of 'while( ( iter != m_PropTree.end() ) && !found)'

	QL_REQUIRE(found, "FXPricesXMLSource::get(..):\n"
		<< "Was unable to find the FX Prices for: " << ID1 << " - " << ID2);  

	return prices;
}

boost::shared_ptr<StockData> StockDataXMLSource::get(const std::string& stockID, const std::string& stockIDType)
{
	QL_REQUIRE( stockID.size() > 0, "StockDataXMLSource::get(..): stockID can't be an empty string.");

	boost::shared_ptr<StockData> stockData;
	bool found = false;

	using boost::property_tree::ptree;
    ptree::const_iterator iter = m_PropTree.begin();
			
	while( ( iter != m_PropTree.end() ) && !found)
	{
		if( iter->first.data() == CONST_STR_stock)
		{   
			std::string IDinData      = iter->second.get<std::string>("id");
			
			boost::optional<std::string> ID_typeInData;

			bool matchingIDTypes = true; 
			// We have two values for ID type, one is passed into this method (stockIDType), 
			// the other (IDTypeInData) is obtained from the xml data. 
			// If either is an empyt string then matchingIDTypes is set to true
			// otherwise mathcingIDTypes = ( IDType == IDTypeInData)
			
			if( ID_typeInData = iter->second.get_optional<std::string>("id_type"))
			{   // have been able to retrieve ID_type from xml data 
				if(stockIDType.size() != 0)
					matchingIDTypes = ( (*ID_typeInData) == stockIDType);
			}   // otherwise leave matchingIDTypes at true.
		
			// matchingIDTypes(..) will return true if either string is empty.
			if((IDinData == stockID) && matchingIDTypes)
			{
				stockData = (boost::shared_ptr<StockData>) new StockData();
				stockData->setID(IDinData);

				if( ID_typeInData )
					stockData->setIDType(*ID_typeInData);
					
				boost::optional<std::string> ccyInData;
				if(ccyInData = iter->second.get_optional<std::string>("currency"))
					stockData->setCurrency(*ccyInData);
				
				boost::optional<std::string> name;
				if(name = iter->second.get_optional<std::string>("name"))
					stockData->setName(*name);

				boost::optional<std::string> holCal;
				if(holCal = iter->second.get_optional<std::string>("holiday_calendar"))
					stockData->setHolidayCalendarID(*holCal);

				boost::optional<Real> flatVol;
				if(flatVol = iter->second.get_optional<Real>("flat_vol"))
					stockData->setFlatVol(*flatVol);

				boost::optional<Real> price;
				if(price = iter->second.get_optional<Real>("price"))
					stockData->setSpotPrice(*price);

				boost::optional<Real> divYield;
				if(divYield = iter->second.get_optional<Real>("dividend_yield"))
					stockData->setDividendYield(*divYield);

				boost::optional<Real> repoRate;
				if(repoRate = iter->second.get_optional<Real>("repo_rate"))
					stockData->setRepoRate(*repoRate);
			
				boost::optional<Real> creditSpread;
				if(creditSpread = iter->second.get_optional<Real>("credit_spread"))
					stockData->setCreditSpread(*creditSpread);

				found  = true;
			}
		}		
		iter++; // keep searching
	}			// end of while(..)
	QL_REQUIRE(found, "StockDataXMLSource::get(..): Was unable to find the stock, " 
		              << stockIDType << " : " << stockID );
	return stockData;
}
