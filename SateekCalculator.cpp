#include "SateekCalculator.hpp"

Calculator::Calculator() {}

Calculator::Calculator(const std::string& pathToXMLPortfolio, const std::string& pathToMarketData) 
: m_portfolio(pathToXMLPortfolio),
  m_marketCaches(pathToMarketData)
{}

Size           Calculator::getNumContracts()  { return m_portfolio.size(); }
MarketCaches*  Calculator::getMarketCaches()  { return &m_marketCaches;    }
Portfolio*     Calculator::getPortfolio()     { return &m_portfolio;       }

void Calculator::evaluateSingleContract(Size        contractNum,  // input
		                                ResultSet*  pResultSet)   // output, new Results are added to the ResultSet
{
	QL_REQUIRE(contractNum < getNumContracts(), 
		       "Calculator::evaluateSingleContract(..): Can't evaluate contract number "
			   << contractNum + 1 << " since we only have " << getNumContracts() << " contracts.");

	Contract* pContract = m_portfolio.get(contractNum);
    ContractCategory  contractCategory =  pContract->getCategory();

	switch (contractCategory) // Below if any of the dynamic_casts fail, then pContract will 
	{   // be set to NULL and that will be checked for in the constructor for CalculatorBase
        // and a (nice) exception will be thrown. There'll be no nasty crash!
		case equity_linked_note: 
			EquityLinkedNoteCalculator(dynamic_cast<EquityLinkedNoteContract*>(pContract), 
				                       &m_marketCaches,  
				                       pResultSet);  // Results will be inserted into the result set. 
		break;
			
		case convertible_bond:
			ConvertibleBondCalculator(dynamic_cast<ConvertibleBondContract*>(pContract), 
				                      &m_marketCaches,  
				                      pResultSet);  // Results will be inserted into the result set. 
		break;

		case koda:
			AccumulatorCalculator(dynamic_cast<AccumulatorContract*>(pContract), 
				                  &m_marketCaches, 
				                  pResultSet); // Results will be inserted into the result set.
		    break;

		case range_accrual:
			RangeAccrualCalculator(dynamic_cast<RangeAccrualContract*>(pContract), 
				                  &m_marketCaches, 
				                  pResultSet); // Results will be inserted into the result set.
		    break;

		case call_spread_cpn_note:
			CallSpreadCpnNoteCalculator(dynamic_cast<CallSpreadCpnNoteContract*>(pContract), 
				                        &m_marketCaches, 
				                        pResultSet); // Results will be inserted into the result set.
		    break;

		default:  
			QL_FAIL("Calculator::evaluateSingleContract(..): Don't have model for contractCategory: "
				    << toString(contractCategory) 
					<< " for contract number " << contractNum + 1 
					<< ", with ID: " << pContract->getID() );
	}
}

void Calculator::writeResultSetToFile(ResultSet* resultSet, const std::string& name, Size contractNum)
{
	std::string usedName, shortOrLong;
        
	if( getConfig()->find("output_filename_short_or_long", shortOrLong)
		&& (!strcmp(shortOrLong.c_str(), "short")))   // when 'short' has been specified in the config
				
		usedName = "res_" + toString(contractNum + 1); // use the shortened name. Counting base: 1.
	else
		usedName = name;  // use the full (long) name

	std::string directory;
	std::string path =  (getConfig()->find("output_directory", directory) ? directory + "/" : "")
			                + usedName + ".xml";

	std::ofstream file;
    file.open(path.c_str());
	if( file.fail())           // Unable to open the file.
	{
		std::string msg = "Was unable to write results to file: " + path 
			             + "\nThe result contained: \n"	+ toString(*resultSet);
		writeDiagnostics(msg, low, "Calculator::writeToFile");

		QL_FAIL("Calculator::writeResultSetToFile(..): Was unable to open file "
			    << path << " for writing results.");
	}
	else                       // Was successful in opening the file.
	{
		file << (*resultSet);  // Write the results to the file.
		file.close();
	    writeDiagnostics("Wrote results to file: " + path, low); 
	}
}

// Outputing the result-set as requested in the config
void Calculator::processResult(ResultSet* resultSet, Size contractNum) 
{
	QL_REQUIRE(resultSet != NULL, "processResult(..): resultSet pointer was NULL.");

	std::string outputStr = getConfig()->get("output");

	std::string catStr, IDStr;
    Date evalDate = m_marketCaches.getEvalDate();

	// The name will contain the eval_date, the contract number, 
	// the contract category and the trade ID all concatonated together.
	std::string name =  toString(evalDate, "yyyy-mm-dd") + "__" + toString(contractNum+1)   + "_"
		              + toString(m_portfolio.get(contractNum)->getCategory())
					  + "_" + m_portfolio.get(contractNum)->getID();

	if( outputStr.find("std_out")       != std::string::npos) // need to send Results to std_out
	{
		writeDiagnostics(" Results for: " + name, low, "Calculator::processResult"); 
		std::cout << *resultSet << std::endl;
	}

	if( outputStr.find("write_to_file") != std::string::npos) // need to write_to_file
		writeResultSetToFile(resultSet, name, contractNum);

	// could add in other 'if' statement to process the results in some other way
	
	if( resultSet->getCount() == 0)
		writeDiagnostics("Warning: For " + name + ", result-set is empty.", 
		                 low, "Calculator::processResults"); 
}

void Calculator::evaluateAndProcessAll()
{
	ResultSet resultSet;

	for( Size i = 0;  i < getNumContracts(); i++)
	{
		resultSet.clear();                     // clear the old results
		evaluateSingleContract(i, &resultSet); // Do the calculation and populate the resultSet
		processResult(&resultSet, i);          // write to file and / or send to std::cout 
		                                       // etc as specified in config.xml 
	}
}


std::string getHelpText(const std::string& exeName)
{
	std::stringstream stream;
	stream << "The sateek calculator: \n" 
           << "Usage: \n"
		   << "   " << exeName << " -h                for help.\n"
		   << "   " << exeName << " -v                for the version string.\n"
	       << "   " << exeName << " <path_to_config>  to specify the path to the xml config file.\n"
		   << "   " << exeName << "                   to use the default config (" 
		   << CONST_STR_config_xml << ")." << std::endl;

	return stream.str();
}

void writeToFile(const std::string& filename, const std::string& content)
{
		std::ofstream file;
        file.open(filename.c_str());
		QL_REQUIRE(!file.fail(), "writeToFile(): was unable to open file " 
			                     << filename << " for writing content:\n" << content);
    	file << content;
		file.close();
}

// Would really like to print out the call-stack as it was when the exception was thrown.
// Then the user would know whence the error came.
void processException(std::exception*            exception, 
					  const std::string&         exeName)
{
	try
	{
		std::string errorMsg = "Error when running " + exeName + ":\n";

		if( exception != NULL )
			errorMsg += exception->what();
		else // exception is NULL
			errorMsg += "Unknown error. Sorry can't give you a more helpful error message.\n";

		std::cout << errorMsg << std::endl;

		std::string outputStr;
		Config* pConfig = setGetConfig(); // May be NULL
		if(pConfig && getConfig()->find("output", outputStr))
		{
			if( outputStr.find("write_to_file") != std::string::npos) // do write to file
			{
				std::string pathToErrorLog, outputDirectory;
				if( getConfig()->find("output_directory", outputDirectory))
					pathToErrorLog = outputDirectory + "/";

				pathToErrorLog += "errorLog.txt";
				writeToFile(pathToErrorLog, errorMsg);
			}
		}
	}
	catch(std::exception& e)
	{
		std::cout << "processException(..): Had the following exception when processing an earlier exception: "
			      << std::endl << e.what() << std::endl;		
		std::cout << getHelpText(exeName);
	}
	catch(...)
	{
		std::cout << "processException(..): Had an unknown error when processing an earlier exception." << std::endl; 
	}	
}

// returns true when all is fine and calculation should proceed.
// Also instantiates the Config that can be obtained by calling getConfig() or setGetConfig()
bool processCommandLineArgs(int            argc,                // input 
							char*          argv[])              // input
{
	std::string pathToConfigXMLFile;

	if(argc > 1)
	{
		if( !strcmp(argv[1], "-h"))
		{
			std::cout << getHelpText(argv[0]) << std::endl;
			return false; // finished
		}
		else if( !strcmp(argv[1], "-v"))
		{
			std::cout << "Version: " << CONST_STR_version << std::endl;
			return false; // finished
		}
		else    // will use the user specified path to config file.
		    pathToConfigXMLFile = argv[1];
	}
	else // use the default config xml file
		pathToConfigXMLFile = CONST_STR_config_xml;

	boost::shared_ptr<Config> pConfig = (boost::shared_ptr<Config>) new Config(pathToConfigXMLFile);
    setGetConfig(pConfig); // setGetConfig(.) has a static member which is a 
	                            // boost::shared_ptr<Config> by calling it here, we ensure that the
	                            // config remains in scope in is deleted only as the program terminates.
    return true;
}

void reportElapsedTime(boost::timer& timer, DiagnosticLevel diagnosticLevel)
{
    Real seconds = timer.elapsed();
    Integer hours = int(seconds/3600);
    seconds -= hours * 3600;
    Integer minutes = int(seconds/60);
    seconds -= minutes * 60;
    std::string msg = "Run completed in ";
    if (hours > 0)
        msg = msg + toString(hours) + " hr ";
    if (hours > 0 || minutes > 0)
        msg = msg + toString(minutes) + " min ";

    msg = msg + toString( seconds ) + " sec."; // deliberately leaving in the mili-secs
                                               // i.e. not rounding seconds to the nearest integer.
	writeDiagnostics(msg, diagnosticLevel, "reportElapsedTime"); 		
}

int main(int argc, char *argv[])
{
	try
	{
		if((argc == 3) && (!strcmp(argv[2], "test")))
			testRig(argc, argv);
		else if( processCommandLineArgs(argc, argv))  // This line will also instantiate the Config,   
		{                                        // which can be retrieved by calling getConfig().
			boost::timer timer;
			Calculator calculator;               // Instantiating and initializing the Calculator.
			calculator.evaluateAndProcessAll();  // This call can write results to xml files and or std::cout
			writeDiagnostics("Have now completed the calculations.", low, "main");
			reportElapsedTime(timer, mid);
		}
		// else terminate
	}
    catch (std::exception &e)
	{   processException( &e,                    argv[0]); }
    catch (...) 
	{   processException((std::exception*) NULL, argv[0]); }

	return 0;
}
