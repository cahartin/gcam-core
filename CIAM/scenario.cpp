/*! 
* \file scenario.cpp
* \ingroup CIAM
* \brief Scenario class source file.
* \author Sonny Kim
* \date $Date$
* \version $Revision$
*/				

#include "Definitions.h"
#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <xercesc/dom/DOM.hpp>

#include "scenario.h"
#include "modeltime.h"
#include "Marketplace.h"
#include "world.h"
#include "xmlHelper.h"
#include "Configuration.h"
#include "LoggerFactory.h"
#include "Logger.h"

using namespace std;
using namespace xercesc;

void writeClimatData(void); // function to write data for climat
#if(__HAVE_FORTRAN__)
extern "C" { void _stdcall CLIMAT(void); };
#endif

extern time_t ltime;
extern ofstream logfile, sdcurvefile;

//! Default construtor
Scenario::Scenario() {
    world = 0;
    modeltime = 0;
    marketplace = new Marketplace();
}

//! Destructor
Scenario::~Scenario() {
    clear();
}

//! Perform memory deallocation.
void Scenario::clear() {
    delete world;
    delete modeltime;
    delete marketplace;
}

//! Return a reference to the modeltime->
const Modeltime* Scenario::getModeltime() const {
    return modeltime;
}

//! Return a constant reference to the goods and services marketplace.
const Marketplace* Scenario::getMarketplace() const {
    return marketplace;
}

//! Return a mutable reference to the goods and services marketplace.
Marketplace* Scenario::getMarketplace() {
    return marketplace;
}

//! Return a constant reference to the world object.
const World* Scenario::getWorld() const {
    return world;
}

//! Return a mutable reference to the world object.
World* Scenario::getWorld() {
    return world;
}

//! Set data members from XML input.
void Scenario::XMLParse( const DOMNode* node ){

    DOMNode* curr = 0;
    DOMNodeList* nodeList;
    string nodeName;

    // assume we were passed a valid node.
    assert( node );

    // set the scenario name
    name = XMLHelper<string>::getAttrString( node, "name" );

    // get the children of the node.
    nodeList = node->getChildNodes();

    // loop through the children
    for ( int i = 0; i < static_cast<int>( nodeList->getLength() ); i++ ){
        curr = nodeList->item( i );
        nodeName = XMLHelper<string>::safeTranscode( curr->getNodeName() );

        if( nodeName == "#text" ) {
            continue;
        }

        else if ( nodeName == "summary" ){
            scenarioSummary = XMLHelper<string>::getValueString( curr );
        }

        else if ( nodeName == "modeltime" ){
            if( modeltime == 0 ) {
                modeltime = new Modeltime();
                modeltime->XMLParse( curr );
                modeltime->set(); // This call cannot be delayed until completeInit() because it is needed first. 
            }
            else {
                cout << "Modeltime information cannot be modified in a scenario add-on." << endl;
            }
        }
        else if ( nodeName == "world" ){
            if( world == 0 ) {
                world = new World();
            }
            world->XMLParse( curr );
        }
        else {
            cout << "Unrecognized text string: " << nodeName << " found while parsing scenario." << endl;
        }
    }
}

//! Finish all initializations needed before the model can run.
void Scenario::completeInit() {

    // Complete the init of the world object.
    assert( world );
    world->completeInit();
}

//! Write object to xml output stream.
void Scenario::toXML( ostream& out ) const {

    // write heading for XML input file
    bool header = true;
    if (header) {
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
        out << "<!-- edited with XMLSPY v5 rel. 2 U (http://www.xmlspy.com)";
        out << "by Son H. Kim (PNNL) -->" << endl;
        out << "<!--XML file generated by XMLSPY v5 rel. 2 U (http://www.xmlspy.com)-->" << endl;
    }

    string dateString = util::XMLCreateDate( ltime );
    out << "<scenario xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"";
    out << " xsi:noNamespaceSchemaLocation=\"C:\\PNNL\\CIAM\\CVS\\CIAM\\Ciam.xsd\"";
    out << " name=\"" << name << "\" date=\"" << dateString << "\">" << endl;

    // increase the indent.
    Tabs::increaseIndent();

    // summary notes on scenario
    Tabs::writeTabs( out );
    out << "<summary>\"SRES B2 Scenario is used for this Reference Scenario\"</summary>" << endl;

    // write the xml for the class members.
    modeltime->toXML( out );
    world->toXML( out );
    // finished writing xml for the class members.

    // decrease the indent.
    Tabs::decreaseIndent();

    // write the closing tag.
    Tabs::writeTabs( out );
    out << "</scenario>" << endl;
}

//! Write out object to output stream for debugging.
void Scenario::toDebugXMLOpen( const int period, ostream& out ) const {

    Tabs::writeTabs( out );
    string dateString = util::XMLCreateDate( ltime );
    out << "<scenario name=\"" << name << "\" date=\"" << dateString << "\">" << endl;

    Tabs::increaseIndent();
    Tabs::writeTabs( out );
    out << "<summary>\"Debugging output\"</summary>" << endl;

    // write the xml for the class members.
    modeltime->toDebugXML( period, out );
    world->toDebugXML( period, out );
    // finished writing xml for the class members.

}

//! Write out close scenario tag to output stream for debugging.
void Scenario::toDebugXMLClose( const int period, ostream& out ) const {

    // decrease the indent.
    Tabs::decreaseIndent();

    // write the closing tag.
    Tabs::writeTabs( out );
    out << "</scenario>" << endl;
}

//! Return scenario name.
string Scenario::getName() const {
    return name; 
}

//! Run the scenario
void Scenario::run(){
	
	Configuration* conf = Configuration::getInstance();
	ofstream xmlDebugStream;

	xmlDebugStream.open( conf->getFile( "xmlDebugFileName" ).c_str(), ios::out );
	
	// Start Model run for the first period.
	int per = 0;
   
   if ( conf->getBool( "CalibrationActive" ) ) {
	   world->setupCalibrationMarkets();
   }

	marketplace->initPrices(); // initialize prices
	marketplace->nullDemands( per ); // null market demands
	marketplace->nullSupplies( per ); // null market supply
	
	// Write scenario root element for the debugging.
	toDebugXMLOpen( per, xmlDebugStream );
	
	world->calc( per ); // Calculate supply and demand
	world->updateSummary( per ); // Update summaries for reporting
	world->emiss_ind( per ); // Calculate global emissions
	
	cout << endl << "Period " << per <<": "<< modeltime->getper_to_yr(per) << endl;
	cout << "Period 0 not solved" << endl;
	logfile << "Period:  " << per << "  Year:  " << modeltime->getper_to_yr(per) << endl;
	// end of first period.
	
    // Print the sector dependencies.
    if( conf->getBool( "PrintSectorDependencies", 0 ) ){
        printSectorDependencies();
    }

	// Loop over time steps and operate model
	for ( per = 1; per < modeltime->getmaxper(); per++ ) {	
		
		// Write out some info.
		cout << endl << "Period " << per <<": "<< modeltime->getper_to_yr( per ) << endl;
    	logfile << "Period:  " << per << "  Year:  " << modeltime->getper_to_yr(per) << endl;
		sdcurvefile << "Period " << per << ": "<< modeltime->getper_to_yr( per ) << endl;
		sdcurvefile << "Market,Name,Price,Supply,Demand,";
		sdcurvefile << "Market,Name,Price,Supply,Demand,";
		sdcurvefile << "Market,Name,Price,Supply,Demand,";
		sdcurvefile << "Market,Name,Price,Supply,Demand,";
		sdcurvefile << "Market,Name,Price,Supply,Demand," << endl;
		
		// Run the iteration of the model.
		marketplace->nullDemands( per ); // initialize market demand to null
		marketplace->nullSupplies( per ); // initialize market supply to null
		marketplace->storeto_last( per ); // save last period's info to stored variables
		marketplace->init_to_last( per ); // initialize to last period's info
		world->initCalc( per ); // call to initialize anything that won't change during calc
		world->calc( per ); // call to calculate initial supply and demand
		marketplace->solve( per ); // solution uses Bisect and NR routine to clear markets
		world->updateSummary( per ); // call to update summaries for reporting
		world->emiss_ind( per ); // call to calculate global emissions
		
		// Write out the results for debugging.
		world->toDebugXML( per, xmlDebugStream );
      
      if( conf->getBool( "PrintDependencyGraphs" ) ) {
         // Print out dependency graphs.
         printGraphs( per );
      }
	}
	
	toDebugXMLClose( per, xmlDebugStream ); // Close the xml debugging tag.
	
	// calling fortran subroutine climat/magicc
	world->calculateEmissionsTotals();
	writeClimatData(); // writes the input text file

#if(__HAVE_FORTRAN__)
    cout << endl << "Calling CLIMAT() "<< endl;
    CLIMAT();
    cout << "Finished with CLIMAT()" << endl;
#endif

    xmlDebugStream.close();
}

/*! \brief A function which print dependency graphs showing fuel usage by sector.
*
* This function creates a filename and stream for printing the graph data in the dot graphing language.
* The filename is created from the dependencyGraphName configuration attribute concatenated with the period.
* The function then calls the World::printDependencyGraphs function to perform the printing.
* Once the data is printed, dot must be called to create the actual graph as follows:
* dot -Tpng depGraphs_8.dot -o graphs.png
* where depGraphs_8.dot is the file created by this function and graphs.png is the file you want to create.
* The output format can be changed, see the dot documentation for further information.
*
* \param period The period to print graphs for.
* \return void
*/
void Scenario::printGraphs( const int period ) const {

    Configuration* conf = Configuration::getInstance();
    string fileName;
    ofstream graphStream;
    stringstream fileNameBuffer;

    // Create the filename.
    fileNameBuffer << conf->getFile( "dependencyGraphName", "graph" ) << "_" << period << ".dot";
    fileNameBuffer >> fileName;

    graphStream.open( fileName.c_str() );
    util::checkIsOpen( graphStream, fileName );

    world->printGraphs( graphStream, period );

    graphStream.close();
}

/*! \brief A function to print a csv file including the list of all regions and their sector dependencies.
* 
* \author Josh Lurz
*/
void Scenario::printSectorDependencies() const {
    Logger* logger = LoggerFactory::getLogger( "SectorDependenciesLogger" );
    world->printSectorDependencies( logger );
}

