#ifndef portfolio_hpp
#define portfolio_hpp


#include "Utilities.hpp"

ContractCategory ContractCategoryStringToEnum(const std::string& str);

class Contract // Base class for all contracts
{
private:
	ContractCategory  m_category;    // this is an enum
	std::string       m_ID;
 
	// Other Contract details should be contained in derived class.
public:
	Contract(ContractCategory category, const std::string& ID);

	ContractCategory  getCategory();
	void              setCategory(ContractCategory category);

	std::string       getID();
	void              setID(const std::string& ID);
   
	virtual ~Contract(); // having a virtual method means this class is polymorphic 
	                     // and so we can call dynamic_cast<.>
};

class Portfolio // this contains a collection (vector) of contracts
{  
private: 
	std::vector<boost::shared_ptr<Contract> > m_contracts;
public:
	Contract* get(Size i);

	void addContract(boost::shared_ptr<Contract> contract);

	void clearAll();

	size_t size();

	void reset(std::vector<boost::shared_ptr<Contract> > vContracts);

	Size resetPortfolioFromXMLFile(const std::string& pathToPortfolioXML); // returns the number of contracts in the portfolio

	Portfolio();
	Portfolio(const std::string& pathToPortfolioXML);
};


#endif