#include "Result.hpp"

std::string toString(ResultCategory e)
{
	switch (e) 
	{
		 case price_per_unit_notional:   return "price_per_unit_notional";
         case cash_price:                return "cash_price";
		 case position_worth:            return "position_worth";
		 case delta_pc:                  return "delta_pc";
		 case delta_1:                   return "delta_1";
		 case delta_shares:              return "delta_shares";
         case gamma_1:                   return "gamma_1";
		 case theta:                     return "theta";

		 default: QL_FAIL("toString(.): Unrecognised enum: " << e);
	}
}

ResultCategory stringToResultCategory(const std::string& resCatAsStr)
{
	ResultCategory cat;

	     if( resCatAsStr == "price_per_unit_notional")  cat = price_per_unit_notional;
	else if( resCatAsStr == "cash_price"             )  cat = cash_price;
	else if( resCatAsStr == "position_worth"         )  cat = position_worth;
	else if( resCatAsStr == "delta_pc"               )  cat = delta_pc;
	else if( resCatAsStr == "delta_1"                )  cat = delta_1;
	else if( resCatAsStr == "case delta_shares"      )  cat = delta_shares;
	else if( resCatAsStr == "gamma_1"                )  cat = gamma_1;
	else if( resCatAsStr == "theta"                  )  cat = theta;
	else    QL_FAIL("Unrecognized result category string: " << resCatAsStr);

	return cat;
}


std::string toString(AttrEnum e)
{
	switch (e) 
	{
         case contract_category:      return "contract_category";
         case contract_id:            return "contract_id";
		 case currency:               return "currency";
		 case eval_date:              return "eval_date";
		 case underlying_id:          return "underlying";

		 default: QL_FAIL("toString(.): Unrecognised enum: " << e);
	}
}

// When a calculator is called it will produced a vector of Results,
// one Result could be say delta another could be cashValue
// Each Result can have attibutes such as such as contractID or currency
Result::Result()
{ 
	m_initialized = false;
}

void Result::setValueAndCategory(ResultCategory category, Real value)
{
	m_initialized   = true;
    m_category      = category;
	m_value         = value;
}

Real Result::getValue() const 
{
	QL_REQUIRE( m_initialized, "Result::getValue(): Can't get the Result before it is initialized");
	return m_value;
}

ResultCategory Result::getCategory() const
{
	QL_REQUIRE( m_initialized, "Result::getcategory(): Can't get the Result category before it is initialized");
	return m_category;
}

void Result::setAttribute(AttrEnum e, std::string attrStr, bool throwIfAlreadyPresent)
{ // when throwIfAlreadyPresent is set to true this method will throw an error 
  // if the attribute is already present

    ResultAttrMap::iterator it = m_attributes.find( e );
    if( it != m_attributes.end() ) // this attribute is already present
    {
       QL_REQUIRE( !throwIfAlreadyPresent, 
		          "Result::setAttribute(..): throwIfAlreadyPresent flag set to true, but when setting the new attribute: " 
		          << toString(e) << " to " << attrStr
		          << " found it was already present and set to " << it->second );

	  it->second = attrStr;
	}
	else // attribute is not already present
	{
   		m_attributes.insert(std::pair<AttrEnum, std::string> (e, attrStr));
	}
}

// If the attribute is present
// then the method will return true and the associated string will be copied to str 
// otherwise it will return false
bool Result::findAttribute(AttrEnum     e,         // input  argument
                           std::string& str) const // output argument
{
   // we don't need to check the m_initialized flag here since 
   // this method will return false when the attribute is not present
   ResultAttrMap::const_iterator it = m_attributes.find( e );
   if( it != m_attributes.end() )     // the (e) attribute is present
   {
       str = it->second;              // want a deep copy
	   return true;
   }
   else                              //  the (e) attribute is not present
       return false;
}

std::vector<AttrEnum> Result::getVectorOfAttrs() const
{
	std::vector<AttrEnum> attrs(m_attributes.size());

	// The efficiency of the following could be improved.
	// But the number of attributes is so low, so the work has been postponed!
    ResultAttrMap::const_iterator it = m_attributes.begin();
	for(size_t i = 0; it != m_attributes.end(); it++, i++)
	{
         attrs[i] = it->first;
	}
    return attrs;
}

std::map<AttrEnum, std::string> Result::getAttrsMap() const             { return m_attributes;  }

void  Result::setAttrsMap(const std::map<AttrEnum, std::string>& attrs) { m_attributes = attrs; }

Result::Result(const Result& resFrom) // copy constructor
{
    setValueAndCategory(resFrom.getCategory(), resFrom.getValue());
	setAttrsMap(resFrom.getAttrsMap());		
}

void Result::serialize(std::ostream& stream) const
{
	QL_REQUIRE(m_initialized, "Result::serialize(.): Can only convert an initialized Result to XML");

	stream << std::endl;
	stream << "  <Result>"     << std::endl;
	stream << "    <category>" << toString(m_category) << "</category>"      << std::endl;
	stream << "    <value>"    << std::setprecision(15) << m_value       << "</value>"         << std::endl;

	for(ResultAttrMap::const_iterator it = m_attributes.begin(); it != m_attributes.end(); it++)
	{
        stream << "    <" << toString(it->first) << ">";
        stream << it->second;
		stream << "</"    << toString(it->first) << ">" << std::endl;
	}

	stream << "  </Result>" << std::endl;
}

std::ostream& operator<< (std::ostream &stream, Result &res)
{
	res.serialize(stream);
	return stream;
}

ResultSet::ResultSet()             : m_results(0)     {} // the default c-tor initializes m_results with 0 elements.

ResultSet::ResultSet(size_t count) : m_results(count) {}

Size ResultSet::getCount() const     { return m_results.size(); }
void ResultSet::clear()              { m_results.clear();       }

void ResultSet::addNewResult( boost::shared_ptr<Result>  res)
{
	m_results.push_back(res);
}

boost::shared_ptr<Result> ResultSet::getResult(Size i) 
{
	QL_REQUIRE(i < getCount(), "ResultSet::getResult(i): i (" << i 
		                      << ") must be less than number of results: " 
							  << getCount());
	return m_results[i];  
}

// If the number of Results with the supplied category is zero, then getValue(..) will throw.
// If the number is 2 or greater and allowSummation is false, then getValue(..) will throw.
Real ResultSet::getValue(ResultCategory category, bool allowSummation) const
{
	Real val = 0;
	Size numResultsFound = 0;
	for(Size i = 0 ; i < m_results.size(); i++)
	{
		if( m_results[i]->getCategory() == category)
		{
			val += m_results[i]->getValue();
			numResultsFound++;
		}
	}
	QL_REQUIRE(numResultsFound > 0, "ResultSet::getValue(..): Didn't find any results with category: "
		       << toString(category));
	
	QL_REQUIRE(allowSummation || (numResultsFound == 1),
		       "ResultSet::getValue(..): Expecting unique result,\nbut found " << numResultsFound 
			   << " results with category " << toString(category) << ", but was not allowed to sum them."); 

	return val;
}

void ResultSet::serialize(std::ostream& stream) const
{
	stream << "<Results>" << std::endl;

    for(size_t i = 0; i < m_results.size(); i++)
	{
		stream << *(m_results[i]);
	}
	stream << "</Results>" << std::endl;        
}

std::ostream& operator<< (std::ostream &stream, ResultSet &res)
{
	res.serialize(stream);
	return stream;
}
