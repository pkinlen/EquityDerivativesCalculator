#include "Portfolio.hpp"
#include "EquityLinkedNote.hpp"
#include "ConvertibleBond.hpp"
#include "Accumulator.hpp"
#include "RangeAccrual.hpp"
#include "CallSpreadCpnNote.hpp"

Size Portfolio::resetPortfolioFromXMLFile(const std::string& portfolioPath) // returns the number of contracts in the portfolio
{
	clearAll();

	using boost::property_tree::ptree;
	// open the file and create a property tree based on the contents
	ptree   propertyTree;
	boost::property_tree::xml_parser::read_xml(portfolioPath, propertyTree, 
											   boost::property_tree::xml_parser::trim_whitespace);

	ptree portfolioTree = propertyTree.get_child("portfolio");

	for (ptree::const_iterator iter = portfolioTree.begin(); iter != portfolioTree.end(); ++iter)
	{
		if(iter->first == CONST_STR_contract)
		{   
			std::string contractCategoryStr = iter->second.get<std::string>("contract_category");
			ContractCategory contractCategoryEnum = ContractCategoryStringToEnum(contractCategoryStr);
			// Note that 'contract' is aboost::shared_ptr, i.e. a smart pointer,
			// so when we use 'new' below, we can be confident that the contract will later 
			// be deleted automatically from memory.
			boost::shared_ptr<Contract> contract;
			switch(contractCategoryEnum)
			{
			   case equity_linked_note:
				   contract = (boost::shared_ptr<Contract>) 
					           new EquityLinkedNoteContract(iter->second);
			   break;

			   case convertible_bond:
				   contract = (boost::shared_ptr<Contract>)
					          new ConvertibleBondContract(iter->second);
			   break;

			   case koda: // KODA is a Knock-Out Decumulator or Accumulator
				   contract = (boost::shared_ptr<Contract>)
					          new AccumulatorContract(iter->second);
			   break;

			   case range_accrual:
				   contract = (boost::shared_ptr<Contract>) 
					          new RangeAccrualContract(iter->second);
			   break;

			   case call_spread_cpn_note:
				   contract = (boost::shared_ptr<Contract>) 
					          new CallSpreadCpnNoteContract(iter->second);
			   break;

			   default: 
				   QL_FAIL("Portfolio::resetPortfolioFromXMLFile(.): In " << portfolioPath 
				  	      << " found unrecognised contract_category: " << contractCategoryStr 
						  << " could try 'equity_linked_note'.");
			}
			m_contracts.push_back(contract); // Add the latest contract to the vector.
		}
	}
	return m_contracts.size();
}

Contract* Portfolio::get(Size i)
{
	QL_REQUIRE( i < m_contracts.size(), 
		        "Portfolio.get(i): i must be less than "
				<< m_contracts.size() << " here it is " << i);

	return &(*m_contracts[i]); // perhaps we could do something other than '&(*' ?
}

void Portfolio::addContract(boost::shared_ptr<Contract> contract)
{   m_contracts.push_back(contract); }

void Portfolio::clearAll()  { m_contracts.clear();       }
size_t Portfolio::size()    { return m_contracts.size(); }

void Portfolio::reset(std::vector<boost::shared_ptr<Contract> > vContracts)
{	m_contracts = vContracts;  }


Portfolio::Portfolio()
{ 
	std::string source = getConfig()->get("portfolio_source");
		
	if( source == CONST_STR_xmlfile)
		resetPortfolioFromXMLFile(getConfig()->get("portfolio_xml_path"));
	// could add in code for other sources here, i.e. have: else if(source == ..)
	else 
		QL_FAIL("Portfolio::Portfolio(.): Unrecognised portfolio_source in the config file: " 
			    << source << ". Could try: 'xmlfile'."); 
}

Portfolio::Portfolio(const std::string& pathToPortfolioXML)
{
	resetPortfolioFromXMLFile(pathToPortfolioXML);
}



Contract::Contract(ContractCategory category, const std::string& ID)
{
	m_category   = category;
	m_ID         = ID;
}

ContractCategory  Contract::getCategory()                          { return       m_category;   }
void              Contract::setCategory(ContractCategory category) { m_category   = category;   }

std::string       Contract::getID()                                { return       m_ID;         }
void              Contract::setID(const std::string& ID)           { m_ID         = ID;         }

Contract::~Contract() {}
