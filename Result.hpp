#ifndef result_hpp
#define result_hpp

#include "Utilities.hpp"

enum ResultCategory 
{
	 price_per_unit_notional  = 1,
     cash_price               = 2,  // price_per_unit_notional * notional
	 position_worth           = 3,  // cash_price * position_size, ( position size can be negative for short positions).
	 delta_pc                 = 10, // This the the percentage delta:
	                                // delta_pc = (dV/dS)* S/N   i.e. the delta hedge position divided by the notional
								    // if the spot and notional are in different currencies then we also multiply
								    // by the fx rate so that delta_pc ends up unitless

     delta_1                   = 11, // delta_1 = (dV/dS) * S / 100, i.e. the cash value of a 1% move in spot
	                                 // quoted in the payoff currency ( so may need to multiply by FX )
	 delta_shares              = 12, // the number of shares required to hedge the position
	 gamma_1                   = 20,
	 theta                     = 25
	 // when you add a new enum here please also add one more in the toString(..) function
};

std::string toString(ResultCategory e);
ResultCategory stringToResultCategory(const std::string& resCatAsStr);

enum AttrEnum // these are the attributes that can be associated with each result
{
	 contract_category  = 1,
     contract_id        = 2,
	 currency           = 3,
	 eval_date          = 4,
	 underlying_id      = 5
     
	 // When you add a new enum here please also add one more 'case' in the toString(..) function.
     // Please do NOT add AttrEnum's 'value' nor 'category'.
	 // They are both already used in the Result XML and  
	 // having them in this enum could mess things up.
};
std::string toString(AttrEnum e);

// When a calculator is called it will produced a vector of Results,
// one Result could be say delta_pc another could be cashValue
// Each Result can have attibutes such as such as contractID or currency
class Result 
{
private:
	bool                                                 m_initialized;
	ResultCategory                                       m_category;   // eg cashValue or delta
    Real                                                 m_value;
	typedef  std::map<AttrEnum, std::string>             ResultAttrMap;
	ResultAttrMap                                        m_attributes; // eg { (contractID, "ZSF123423"), (currency, "USD") }
public:
	Result();
	Result(const Result& resFrom); // copy constructor

	void setValueAndCategory(ResultCategory category, Real value);

	Real getValue() const; 

	ResultCategory getCategory() const;

    void setAttribute(AttrEnum e, std::string attrStr, bool throwIfAlreadyPresent = false);

	// If the attribute is present
	// then the method will return true and the associated string will be copied to str 
	// otherwise it will return false
	bool findAttribute(AttrEnum     e,         // input  argument
		               std::string& str) const;// output argument

	std::vector<AttrEnum>            getVectorOfAttrs() const;
	std::map<AttrEnum, std::string>  getAttrsMap()      const;         

	void setAttrsMap(const std::map<AttrEnum, std::string>& attrs);
	void serialize  (std::ostream& stream) const;
};

std::ostream& operator<< (std::ostream &stream, Result &res);

class ResultSet
{
private:
	std::vector<boost::shared_ptr<Result> >   m_results;
public:
	ResultSet();      // the default c-tor initializes m_results with 0 elements.
	ResultSet(size_t count);
	
	Size getCount() const;
	void clear(); 
	void addNewResult( boost::shared_ptr<Result>  res);
	boost::shared_ptr<Result> getResult(Size i);
	void serialize(std::ostream& stream) const;

	// If the number of Results with the supplied category is zero, then getValue(..) will throw.
	// If the number is 2 or greater and allowSummation is false, then getValue(..) will throw.
	Real getValue(ResultCategory category, bool allowSummation = false) const;
}; 

std::ostream& operator<< (std::ostream &stream, ResultSet &res);

#endif 