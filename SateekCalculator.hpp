#ifndef sateekcalculator_hpp
#define sateekcalculator_hpp

#include "Utilities.hpp"
#include "Result.hpp"
#include "Portfolio.hpp"
#include "MarketData.hpp"
#include "EquityLinkedNote.hpp"
#include "ConvertibleBond.hpp"
#include "Accumulator.hpp"
#include "RangeAccrual.hpp"
#include "CallSpreadCpnNote.hpp"
#include "TestRig.hpp"

class Calculator
{
private:
	MarketCaches              m_marketCaches;  // The market caches also contain pointers to the market data source
	Portfolio                 m_portfolio;     // The portfolio is a vector of contracts

public:
	Calculator(); // will get the pathToXMLPortfolio and pathToMarketData from the config
	Calculator(const std::string& pathToXMLPortfolio, const std::string& pathToMarketData);
	Size           getNumContracts();  
	MarketCaches*  getMarketCaches();  
	Portfolio*     getPortfolio();

	void evaluateSingleContract(Size        contractNum,  // input
		                        ResultSet*  pResultSet);  // output, new Results are added to the ResultSet

	void writeResultSetToFile(ResultSet* resultSet, const std::string& name, Size contractNum);

	// Outputing the result-set as requested in the config
	void processResult(ResultSet* resultSet, Size contractNum); 
	void evaluateAndProcessAll();
};

std::string getHelpText(const std::string& exeName);

void writeToFile(const std::string& filename, const std::string& content);

// Would really like to print out the call-stack as it was when the exception was thrown.
// Then the user would know whence the error came.
void processException(std::exception*            exception, 
					  const std::string&         exeName);


void reportElapsedTime(boost::timer& timer, DiagnosticLevel diagnosticLevel);


#endif // ifndef sateekcalculator_hpp
