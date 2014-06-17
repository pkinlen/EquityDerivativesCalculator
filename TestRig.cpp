#include "TestRig.hpp"
#include "Result.hpp"
#include "SateekCalculator.hpp"
#include <ctime>

class Tester
{
private:
	std::string                     m_pathToConfig;
	std::string                     m_pathToMarketData;
	boost::shared_ptr<MarketCaches> m_marketCaches;
	std::vector<std::string>        m_vectTestDetailsPaths;
	Size                            m_currentFileNum;
	std::string                     m_currentTestID;
	std::string                     m_outputDirectory;
public:
	Tester(const std::string& pathToTestSpecification);
	void runAllTests();

	// runTwoLeggedTest(.) returns the number of comparisons completed, throws on failure.
	Size runTwoLeggedTest(const boost::property_tree::ptree& pt);

	// compareResults(..) returns the number of comparisons completed, throws on failure.
	Size compareResults(const ResultSet& leftResultSet, const ResultSet& rightResultSet,
		                const boost::property_tree::ptree& pt); // will throw on failure

	void runOneLegOfTest(const std::string&  pathToConfig, 
	                 	 const std::string&  pathToMarketData, 
		                 const std::string&  pathToContract,
		                 ResultSet*          resultSet);          // output
};

// A tolerance (tol) of zero is allowed
bool equalWithTol(Real leftVal, Real rightVal, Real tol)
{  // suppose leftVal = 1e-20 and rightVal = 1e-30, with tol = 1e-6, then this will pass
	QL_REQUIRE((0 <= tol) && (tol < 1), "equalWithTol(..): Tolerance must be between 0 (inc) and 1 (exclusive),\n"
		       "here it is: " << tol); 

	Real factor = std::max(1.0, std::max(fabs(leftVal), fabs(rightVal)));
	return ((tol * factor) >=  fabs( leftVal - rightVal) );
}

// A tolerance (tol) of zero is allowed
bool greaterThanWithTol(Real leftVal, Real rightVal, Real tol)
{
	QL_REQUIRE((0 <= tol) && (tol < 1), "greaterThanWithTol(..): Tolerance must be between 0 (inc) and 1 (exclusive),\n"
		       "here it is: " << tol); 

	Real factor = std::max(1.0, std::max(fabs(leftVal), fabs(rightVal)));
	return ((leftVal + tol * factor) > rightVal);
}

// A tolerance (tol) of zero is allowed
bool lessThanWithTol(Real leftVal, Real rightVal, Real tol)
{
	return greaterThanWithTol(rightVal, leftVal, tol); // we've swapped the left and right
}

void doComparison(Real leftVal, Real rightVal, const std::string& comparison, Real tol,
				  const std::string& msg) // throws on failure
{
	bool res;
	if(comparison == "equal")
		res = equalWithTol(leftVal, rightVal, tol);
	else if(comparison == "greater_than")
		res = greaterThanWithTol(leftVal, rightVal, tol);
	else if(comparison == "less_than")
		res = lessThanWithTol(leftVal, rightVal, tol);
	else
		QL_FAIL("doComparison(..): Unrecognized comparison string: " << comparison 
		        << ".\nCould try 'equal', 'greater_than' or 'less_than'");

	QL_REQUIRE(res, "doComparison(..): Comparison failed for: " << msg 
		       << "\nleft val:   " << leftVal 
		       << "\ncomparison: " << comparison
		       << "\nright val:  " << rightVal
			   << "\ntolerance:  " << tol);
}

Tester::Tester(const std::string& pathToTestSpecification)
{
	using boost::property_tree::ptree;
	ptree  testSpecPTree;
	boost::property_tree::xml_parser::read_xml(pathToTestSpecification, testSpecPTree, 
										   boost::property_tree::xml_parser::trim_whitespace);
	m_pathToConfig = pt_get<std::string>( testSpecPTree, "test_specification.default_config");
	m_outputDirectory = pt_get<std::string>( testSpecPTree, "test_specification.output_directory");
	boost::shared_ptr<Config> config = (boost::shared_ptr<Config>) new Config(m_pathToConfig);
    setGetConfig(config);
    // writeDiagnostics will keep a static instance of the class Diagnostics
	// that will contain a map of all the different diagnostic levels that are contained in the
	// initial config. The diagnostic settings in the subsequent configs will be ignored.
	writeDiagnostics("Path to test specification is: " + pathToTestSpecification, mid, "Tester");

	pt_getVector<std::string>(testSpecPTree, "test_specification.test_details", CONST_STR_item, // inputs
		                      m_vectTestDetailsPaths);                                          // output

	writeDiagnostics("Found the following test details:\n" + toString(m_vectTestDetailsPaths), 
		             high, "Tester");
}

// returns the number of comparisons completed, throws on failure.
Size Tester::compareResults(const ResultSet& leftResultSet, const ResultSet& rightResultSet,
		                    const boost::property_tree::ptree& pt) // will throw on failure
{

	Size comparisonsDone = 0;
	writeDiagnostics("Comparing results with: \nleft result:\n" + toString(leftResultSet)
		             + "right result:\n"                        + toString(rightResultSet) 
					 + "and property tree:\n"                   + toString(pt),
					 high, "Tester::compareResults");

	boost::property_tree::ptree::const_iterator iter = pt.begin();
    while(iter != pt.end())
    {  
	    if(iter->first == "comparison")
		{
			std::string categoryStr = pt_get<std::string>(iter->second, "category");
			ResultCategory category = stringToResultCategory(categoryStr);
			Real leftVal = leftResultSet.getValue(category);
	
			std::string comparison  = pt_get<std::string>(iter->second, "comparison_type");
			Real        tol         = pt_get<Real>       (iter->second, "tolerance");    
			std::string rightSource = pt_get<std::string>(iter->second, "right_source");

			Real rightVal;
			if(rightSource == "right_leg")
				rightVal = rightResultSet.getValue(category);
			else if( rightSource == "constant")
				rightVal = pt_get<Real>(iter->second, "constant");
			else QL_FAIL("'right_source' must be either 'right_leg' or 'constant'. Here it is: " 
				         << rightSource);

			std::string msg = m_currentTestID + "\ncategory:   " + categoryStr; // used when exception is thrown
			doComparison(leftVal, rightVal, comparison, tol, msg); // throws on failure
			comparisonsDone++;
		}
	    iter++;
    }    		
    // when there are no comparisons done, i.e. comparisonsDone == 0,
	// there may have been a problem readin the xml
	// so we will use a diagnostic level of 'low'
	// making it more likely the message will be shown to the user.
	writeDiagnostics("For test: " + m_currentTestID 
		             + ",\nthe number of comparisons done was: " + toString(comparisonsDone)+ "\n", 
		             comparisonsDone > 0 ? high : low, "Tester::compareResults");

	return comparisonsDone;
}

void Tester::runOneLegOfTest(const std::string&  pathToConfig, 
	                 	     const std::string&  pathToMarketData, 
		                     const std::string&  pathToContract,
		                     ResultSet*          resultSet)          // output
{
	writeDiagnostics("test id: " + m_currentTestID 
		             + "\npath to config: "     + pathToConfig
           		     + "\npath to marketData: " + pathToMarketData
		             + "\npath to contract: "   + pathToContract,
					 high, "Tester::runOneLegOfTest");

	if(pathToConfig != m_pathToConfig) // we have a new config
	{
		m_pathToConfig = pathToConfig;
		boost::shared_ptr<Config> config = (boost::shared_ptr<Config>) new Config(m_pathToConfig);
		setGetConfig(config);
	}
	// else use existing config

	Calculator calculator(pathToContract, pathToMarketData);
	calculator.evaluateSingleContract(0, resultSet);	
}

// returns the number of comparisons completed, throws on failure.
Size Tester::runTwoLeggedTest(const boost::property_tree::ptree& testPTree)
{ 
	m_currentTestID = pt_get<std::string>(testPTree, "test_id");
	writeDiagnostics("About to start test ID: " + m_currentTestID, mid, "Tester::runTwoLeggedTest");

	ResultSet leftResultSet, rightResultSet;
	std::string leftConfig     = pt_get<std::string>(testPTree, "left_leg.path_to_config");
	std::string leftMarketData = pt_get<std::string>(testPTree, "left_leg.path_to_market_data");
	std::string leftContract   = pt_get<std::string>(testPTree, "left_leg.path_to_contract");

	runOneLegOfTest(leftConfig, leftMarketData, leftContract, // inputs
		             &leftResultSet);                          // output

	std::string rightConfig = pt_get_optional<std::string>(testPTree, "right_leg.path_to_config", "");
	std::string rightMarketData, rightContract;

	bool hasRightLeg;
	if(hasRightLeg = (rightConfig.length() > 0))
	{
		rightMarketData = pt_get<std::string>(testPTree, "right_leg.path_to_market_data");
		rightContract   = pt_get<std::string>(testPTree, "right_leg.path_to_contract");

	    runOneLegOfTest(rightConfig, rightMarketData, rightContract, // inputs
		                 &rightResultSet);                           // output
	}
	else 
		writeDiagnostics("For test ID: " + m_currentTestID + " found no right leg.", 
		                 mid, "Tester::runTwoLeggedTest"); 

	return compareResults(leftResultSet, rightResultSet, testPTree);
}
// the following will help with lexicographical sorting. i.e. "04" comes before "11"
// (on the other hand "4" comes after "11")
std::string toStringWithTwoChars(int i) // will convert int 8 to "08"
{
	QL_REQUIRE((0 <= i ) && (i <= 99), "toStringWithTwoChars(.): int outside range 0 to 99 inclusive: " << i);

	std::string intAsStr = toString(i);
	if(intAsStr.length() == 1)   // we prepend strings such as "8" with "0" to make "08"
		intAsStr = "0" + intAsStr;

	return intAsStr;
}

std::string	getResultFilename()
{	
	time_t t = time(NULL); 
	struct tm* timeInfo  = localtime(&t);
	std::string filename = "test_results_"  
		                   + toString(timeInfo->tm_year+ 1900)          + "-"
		                   + toStringWithTwoChars(timeInfo->tm_mon + 1) + "-" 
                           + toStringWithTwoChars(timeInfo->tm_mday)    + "_"
						   + toStringWithTwoChars(timeInfo->tm_hour)    + "h"
						   + toStringWithTwoChars(timeInfo->tm_min)     + "m"
						   + toStringWithTwoChars(timeInfo->tm_sec)     + "s.txt";
	return filename;
}

void Tester::runAllTests() 
{
	std::string testSummary;
	Size        numberOfTests = 0;
    Size        numberOfComparisons = 0;

	for(m_currentFileNum = 0; m_currentFileNum < m_vectTestDetailsPaths.size(); m_currentFileNum++)
	{
		testSummary += "\nTest Details File: " + m_vectTestDetailsPaths[m_currentFileNum] + toString("\n");
		boost::property_tree::ptree testDetailsPTree;
		boost::property_tree::xml_parser::read_xml(m_vectTestDetailsPaths[m_currentFileNum], testDetailsPTree, 
											       boost::property_tree::xml_parser::trim_whitespace);

	    boost::property_tree::ptree::const_iterator iter = testDetailsPTree.get_child("test_details").begin();
		
	    while(iter != testDetailsPTree.get_child("test_details").end())
	    {  
		    if(iter->first == "test")
			{
			    numberOfComparisons += runTwoLeggedTest(iter->second);
				numberOfTests++;
				testSummary += toString("    ") + pt_get<std::string>(iter->second, "test_id") + "\n";
			}
  		    iter++;
	    }    
	}
	std::string outputPath = m_outputDirectory + "/" + getResultFilename();
	writeToFile(outputPath, "Successfully completed all " + toString(numberOfTests) + " tests:\n" + testSummary);
	writeDiagnostics("Wrote test summary to file: " + outputPath, mid, "Tester::runAllTests"); 
	writeDiagnostics("Successfully completed all " + toString(numberOfTests) + " test.", low, "Tester::runAllTests"); 
	writeDiagnostics("Had a total of " + toString(numberOfComparisons) + " comparisons.", mid, "Tester::runAllTests");
}

void testRig(int argc, char *argv[]) // will throw if it fails
{
	QL_REQUIRE((argc == 3) && !strcmp(argv[2], "test"), 
		       "test rig must be called with 3 arguments: \n"
		       << "<executible_filename>  <path_to_test_config> test" );

    std::string path_to_default_test_specification = "c:/sateek/test/test_specification.xml";
    // The test below (argc > 1) will always pass because of the QL_REQUIRE(..) above.
	// But we keep it in because we may remove the QL_REQUIRE at some point in the future 
	Tester tester( argc > 1 ? toString(argv[1]) : path_to_default_test_specification);
    tester.runAllTests();
}
