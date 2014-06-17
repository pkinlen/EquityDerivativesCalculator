#ifndef marketdata_hpp
#define marketdata_hpp

#include "Utilities.hpp"

class MarketCaches; // forward declaration

template<class Obj> class MarketObjSource
{
protected:
	MarketCaches*                  m_marketCaches;   // deliberately a pointer rather than smart-pointer	
	bool                           m_usingSingleKey; // when false it means using dual-key
public:
	MarketObjSource(MarketCaches*  marketCaches, bool usingSingleKey = false);

	virtual Obj get(const std::string& key1, const std::string& key2)
	{ 
		QL_FAIL("get(key1, key2), not implemented in the base class MarketObjSource.\nHere called with: "
			    << key1 << ", " << key2 << ". ");
		return *((Obj*) NULL); // The QL_FAIL(.) in the line above will prevent this line being reached.
	}                          // So no need to worry about an access violation.

	virtual Obj get_s(const std::string& key) 
	{ 
		QL_FAIL("get_s(key), not implemented in the base class MarketObjSource.\nHere called with: "
			    << key << ". Perhaps the m_usingSingleKey flag was incorrectly set to true?");
		return *((Obj*) NULL); // The QL_FAIL(.) in the line above will prevent this line being reached.
	}                          // So no need to worry about an access violation.

	// In this base class we break up the key into two 
    virtual Obj get(const std::string& key)
	{
		if(m_usingSingleKey)
			return get_s(key); // want the derived class method to be called
		else
		{
			Size pos = key.find(CONST_STR_divider);
			QL_REQUIRE( pos != std::string::npos, 
				"MarketObjSource::get(key): When using a single key, "
						<< "it must be of the form key1" << CONST_STR_divider << "key2, eg 'GBP"
						<< CONST_STR_divider << "USD'. Here got: " << key);

			std::string key1 = key.substr( 0, pos);
			std::string key2 = key.substr( pos + CONST_STR_divider.size() );
			return get( key1, key2); // this will call the get(.,.) in the derived class
		}
	}

};

template<class T_Obj>
class MarketObjXMLSource : public MarketObjSource<T_Obj> // base class for classes that generate
{                                                        // market data objects from XML.
protected:
	boost::property_tree::ptree    m_PropTree;    
public:
	MarketObjXMLSource(MarketCaches*         marketCaches,
		               const std::string&    tagOfFilenameInConfig, 
					   const std::string&    pathInPropertyTree);
};

// Very often the Obj will be a boost::shared_ptr<.>, though it could also be a Real
template<class Obj> class CacheSingleKey 
{   
protected:
	boost::shared_ptr<MarketObjSource<Obj> >    m_source;
	std::map<std::string, Obj>                  m_map;
public:
	CacheSingleKey( boost::shared_ptr<MarketObjSource<Obj> > source );

	// get an object from the cache, if it's not there then go to the source and add it to the cache
	Obj get(const std::string& key);
	
	// We don't have an add(.) method here since for the FX we want FXSpot(X,Y) = 1/FXSpot(Y,X)
	// we'll let the source achieve this.
	void clear();
};

// Very often the Obj will be a boost::shared_ptr<.>, though it could also be a Real
template<class Obj> class CacheDualKey : public CacheSingleKey<Obj>
{ // In this class we concatonate the two keys with "<divider>" in the middle, 
  // then we'll use the class: CacheSingleKey
public:
	CacheDualKey( boost::shared_ptr<MarketObjSource<Obj> > source);
	
	// get an object from the cache, if it's not there go to the source and add it to the cache
	Obj get(const std::string& key1, const std::string& key2);
};

class FXVolXMLSource : public MarketObjXMLSource<boost::shared_ptr<BlackVolTermStructure> >
{
public:
	FXVolXMLSource(MarketCaches* marketCaches);

	// get(..) will throw an error if the FXVol is not found.
	boost::shared_ptr<BlackVolTermStructure> get(const std::string& currency1, const std::string& currency2);
};

class YieldTS_XMLSource : public MarketObjXMLSource<boost::shared_ptr<YieldTermStructure> >
{
public:
	YieldTS_XMLSource(MarketCaches* marketCaches);

	// get(..) will throw an error if the Object is not found.
	boost::shared_ptr<YieldTermStructure> get(const std::string& yieldType, const std::string& currency);
};

// contains historic and current prices
class Prices
{
private:
	std::map<Date, Real>   m_mapOfPrices;
	bool                   m_currentPriceIsSet;
	Real                   m_currentPrice;
	Date                   m_evalDate;
	std::string            m_ID1, m_ID2;                  // useful for error messages
public:
	Prices(Date evalDate, const std::string& ID1 = "", const std::string& ID2 = "");

	void addPrice          (const Date&  date,      Real price);
	void setCurrentPrice   (Real currentPrice);

	bool findPrice         (const Date&  date,            // input,         returns false if the price is absent
		                          Real&  price) const;    // output

	Real getPrice          (const Date&  date)  const;    // throw's when absent
	Real getCurrentPrice   ()                   const;    // throw's when absent
};

class StockData
{
private:
	std::string   m_ID;
	std::string   m_IDType;
	std::string   m_name;
	std::string   m_holidayCalendarID;

	std::string   m_currency;
	Real          m_flatVol;
	Real          m_dividendYield;
    Real          m_repoRate;        // Repo rate causes the forward to decrease.
	                                 // It can be used to adjust the forward.
	Real          m_spotPrice;
	Real          m_creditSpread;
	bool          m_creditSpreadIsSet;
public:
	StockData(); 

	///////////////////////////////////////////////////////////////////////
	// get methods
	std::string  getID(); 
	std::string  getIDType();      
	std::string  getName();
	std::string  getHolidayCalendarID();
	std::string  getCurrency();
	Real         getDividendYield();
	Real         getRepoRate();
	Real         getFlatVol();		
	Real         getCreditSpread();

	///////////////////////////////////////////////////////////////////////
	// set methods
	void setID                (const std::string& ID          );
	void setIDType            (const std::string& IDType      );
	void setName              (const std::string& name        );
	void setHolidayCalendarID (const std::string& cal         );
	void setCurrency          (const std::string& currency    );
	void setFlatVol           (Real               flatVol     );
	void setSpotPrice         (Real               spotPrice   );
	void setDividendYield     (Real               divYield    );
	void setRepoRate          (Real               repoRate    );
	void setCreditSpread      (Real               creditSpread);
};
////////////////////////////////////////////////////////////////////////////////////
class StockDataXMLSource : public MarketObjXMLSource<boost::shared_ptr<StockData> >
{
public:
	StockDataXMLSource(MarketCaches* marketCaches);

	// get(..) will throw an error if the Object is not found.
	boost::shared_ptr<StockData> get(const std::string& stockID, const std::string& stockIDType);
};
////////////////////////////////////////////////////////////////////////////////////
class StockPricesXMLSource : public MarketObjXMLSource<boost::shared_ptr<Prices> >
{
public:
	StockPricesXMLSource(MarketCaches* marketCaches);
	
	// get(..) will throw an error if the Object is not found.
	boost::shared_ptr<Prices> get(const std::string& ID, const std::string& IDType);
};
////////////////////////////////////////////////////////////////////////////////////
class FXPricesXMLSource : public MarketObjXMLSource<boost::shared_ptr<Prices> >
{
public:
	FXPricesXMLSource(MarketCaches* marketCaches);
	
	// get(..) will throw an error if the Object is not found.
	boost::shared_ptr<Prices> get(const std::string& ID1, const std::string& ID2);
};
////////////////////////////////////////////////////////////////////////////////////

class CalendarXMLSource : public MarketObjXMLSource<boost::shared_ptr<Calendar> >
{
public:
	CalendarXMLSource(MarketCaches* marketCaches);

	// get(..) will throw an error if the Object is not found.
	boost::shared_ptr<Calendar> get_s(const std::string& calID); // , const std::string& ignoredParameter = "");
};

class MarketCaches
{   // Objects such as Yield-term-structures are obtained from 'sources'
	// and the objects are stored in caches
	// This class contains a collection of caches.
	// When a cache is asked for an object it will check if it has it already
	// if it does, then it will be returned.
	// otherwise it will get it from the source, store it and return a shared_ptr to it.

	// If a developer wants to use a new data source, the cache classes don't need to be changed,
    // but we do need a new class derived from MarketObjSourceBase.
private:
	std::string										m_XMLPath;	
	boost::property_tree::ptree                     m_XMLPropTree; // This data object is potentially large   
	Date                                            m_evalDate;

    // The getCache(..) method is a private templated method used to retrieve the cache of data objects.
	// The objects (T_Obj) come from the source and are stored in the cache.
	// If the Cache has not already been created the this method will create it
	// Either way a shared pointer to the cache will be returned.
	template<class T_Obj,       // This is the object that will be created, perhaps a vol object, or yield-term-structure
	         class T_XMLSource, // When xml is being use, this is the class that will create the object from the xml data
	         class T_Cache>	    // Using 'T_' here to indicate a templated type.
	boost::shared_ptr<T_Cache>  getCache(// The tag used in the config to specify the data source
                                         const std::string&           sourceTagInConfig,     
			    	                     // The key used in the config to specify the data source
					                     boost::shared_ptr<T_Cache >*  ptrToSharedPtrToCache)
	{
		typedef boost::shared_ptr < T_Obj       >  SharedPtrToObj; 
		typedef boost::shared_ptr < T_Cache     >  SharedPtrToCache;
		typedef boost::shared_ptr < T_XMLSource >  SharedPtrToXMLSource;

		if( (*ptrToSharedPtrToCache) == NULL ) // Cache not yet created, so we create it now.
		{
			std::string sourceStr;

			// when the source has not be specified in the config we use the market_data_source_default
			if( !( getConfig()->find(sourceTagInConfig, sourceStr) ) )
				sourceStr = getConfig()->get(CONST_STR_market_data_source_default);

			if( sourceStr == CONST_STR_xmlfile) // The source specified in the config is XML.
			{   // when we're using an xmlfile as the source we set the source to the class: T_XMLSource.
				SharedPtrToXMLSource    sharedPtrToXMLSource;
				sharedPtrToXMLSource = (SharedPtrToXMLSource) new T_XMLSource (this);

				// This method is usually called with ptrToSharedPtrToCache pointing to a member variable.
				// When that is the case, the following line will set that member variable.
				(*ptrToSharedPtrToCache) = (SharedPtrToCache) new T_Cache(sharedPtrToXMLSource);
			}
			// when we want to implement a data source other than xml,
			// here we would need: else if ( sourceStr == ... ) ...
			else
			{
				QL_FAIL("MarketCaches::getCache(..): Unrecognised source: " 
						<< sourceStr << ". Could try use market_date_source_default = 'xmlfile'.");
			}
		}
		return (*ptrToSharedPtrToCache);			
	}

public:
	// The caches will be initialized only when they are requested.
	MarketCaches();  // Constructor.
    MarketCaches(const std::string& pathToXMLMarketData);

    void initialize();

	// The following method will only be used when an XML market data source is used.
	boost::property_tree::ptree* getXMLPropTree();

	Date getEvalDate();
	
///////////////////////////////////////////////////////////////////////////////
// The FXVol Cache
		   typedef boost::shared_ptr<BlackVolTermStructure>            FXVolSmartPointer;
		   typedef boost::shared_ptr<CacheDualKey<FXVolSmartPointer> > FXVolCacheSmartPointer;
private:   FXVolCacheSmartPointer                                      m_FXVolCache;
public:    FXVolCacheSmartPointer                                      getFXVolCache();

///////////////////////////////////////////////////////////////////////////////
// Yield Term Structure Cache
		   typedef boost::shared_ptr<YieldTermStructure>                 YieldTSSmartPointer;
		   typedef boost::shared_ptr<CacheDualKey<YieldTSSmartPointer> > YieldTSCacheSmartPointer;
private:   YieldTSCacheSmartPointer m_YieldTSCache;
public:    YieldTSCacheSmartPointer getYieldTSCache();

///////////////////////////////////////////////////////////////////////////////
// Stock Data Cache
		   typedef boost::shared_ptr<StockData>                             StockDataSharedPointer;
		   typedef boost::shared_ptr<CacheDualKey<StockDataSharedPointer> > StockDataCacheSharedPointer;
private:   StockDataCacheSharedPointer  m_stockDataCache;
public:    StockDataCacheSharedPointer  getStockDataCache();
///////////////////////////////////////////////////////////////////////////////
// Stock Prices Cache
		   typedef boost::shared_ptr<Prices>                              PricesSharedPointer;
		   typedef boost::shared_ptr<CacheDualKey<PricesSharedPointer> >  StockPricesCacheSharedPointer;
private:   StockPricesCacheSharedPointer  m_stockPricesCache;
public:    StockPricesCacheSharedPointer  getStockPricesCache();
///////////////////////////////////////////////////////////////////////////////
// FX Prices Cache
		   typedef boost::shared_ptr<CacheDualKey<PricesSharedPointer> > FXPricesCacheSharedPointer;
private:   FXPricesCacheSharedPointer  m_FXPricesCache;
public:    FXPricesCacheSharedPointer  getFXPricesCache();
///////////////////////////////////////////////////////////////////////////////
// Calendar Cache
		   typedef boost::shared_ptr<Calendar>                                CalendarSharedPointer;
		   typedef boost::shared_ptr<CacheSingleKey<CalendarSharedPointer> >  CalendarCacheSharedPointer;
private:   CalendarCacheSharedPointer  m_calendarCache;
public:    CalendarCacheSharedPointer  getCalendarCache();
///////////////////////////////////////////////////////////////////////////////

};  // end of class MarketCaches


template<class Obj>
MarketObjSource<Obj>::MarketObjSource(MarketCaches*  marketCaches, bool usingSingleKey)
{
	m_marketCaches    = marketCaches;
	m_usingSingleKey  = usingSingleKey; // when false will use dual key
}

template<class Obj>
MarketObjXMLSource<Obj>::MarketObjXMLSource(MarketCaches*         marketCaches,
										    const std::string&    tagOfFilenameInConfig, 
										    const std::string&    pathInPropertyTree)
		: MarketObjSource<Obj>(marketCaches)
{
	std::string filename;
    if( getConfig()->find(tagOfFilenameInConfig, filename)) // When there is a specific file for this object,
    {
    	boost::property_tree::ptree propTree;
	    boost::property_tree::xml_parser::read_xml(filename, 
			                                       propTree,
												   boost::property_tree::xml_parser::trim_whitespace);
	    m_PropTree = propTree.get_child(pathInPropertyTree);
	}
	else // There isn't a specific file for this object
	{
		// we'll get the data from the main market data xml.
		m_PropTree = this->m_marketCaches->getXMLPropTree()->get_child(pathInPropertyTree);
	}	
}

template<class Obj>
CacheSingleKey<Obj>::CacheSingleKey( boost::shared_ptr<MarketObjSource<Obj> > source )
{ 
	m_source        = source; 
}

// get an object from the cache, if it's not there then go to the source and add it to the cache
template<class Obj>
Obj CacheSingleKey<Obj>::get(const std::string& key)
{
	QL_REQUIRE(key.size() > 0, "CacheSingleKey::get(.): Can't have an empty string as a key.");

	typename std::map<std::string, Obj>::iterator iter = m_map.find( key);

	if( iter != m_map.end())          // We have this object already in the cache.
		return iter->second;    
	else                              // The object is not already in the cache.
	{
		Obj obj = m_source->get(key); // this may throw
		m_map.insert(std::pair<std::string, Obj>(key, obj));
		return obj;
	}
}
	
// We don't have an add(.) method here since for the FX we want FXSpot(X,Y) = 1/FXSpot(Y,X)
// we'll let the source achieve this.
template<class Obj>
void CacheSingleKey<Obj>::clear() 
{ 
	m_map.clear(); 
}

template<class Obj>
CacheDualKey<Obj>::CacheDualKey( boost::shared_ptr<MarketObjSource<Obj> > source) 
   : CacheSingleKey<Obj>(source) 
{}

// get an object from the cache, if it's not there go to the source and add it to the cache
template<class Obj>
Obj CacheDualKey<Obj>::get(const std::string& key1, const std::string& key2)
{
	QL_REQUIRE( (key1.size() + key2.size()) > 0, "CacheDualKey::get(..): Found both keys to be empty.") 
	return CacheSingleKey<Obj>::get( key1 + CONST_STR_divider + key2); // will use the single key base class
}



#endif // for  #ifndef marketdata_hpp
