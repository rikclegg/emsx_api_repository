/* Copyright 2017. Bloomberg Finance L.P.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:  The above
* copyright notice and this permission notice shall be included in all copies
* or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/
#include "BlpThreadUtil.h"

#include <blpapi_correlationid.h>
#include <blpapi_element.h>
#include <blpapi_event.h>
#include <blpapi_message.h>
#include <blpapi_name.h>
#include <blpapi_session.h>
#include <blpapi_subscriptionlist.h>

#include <cassert>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <time.h>
#include <vector>

using namespace BloombergLP;
using namespace blpapi;

namespace {

	Name SESSION_STARTED("SessionStarted");
	Name SESSION_STARTUP_FAILURE("SessionStartupFailure");
	Name SERVICE_OPENED("ServiceOpened");
	Name SERVICE_OPEN_FAILURE("ServiceOpenFailure");
	Name ERROR_INFO("ErrorInfo");
	Name CREATE_ORDER_AND_ROUTE_MANUALLY("CreateOrderAndRouteManually");

	const std::string d_service("//blp/emapisvc_beta");
	CorrelationId requestID;

}

class ConsoleOut
{
private:
	std::ostringstream  d_buffer;
	Mutex              *d_consoleLock;
	std::ostream&       d_stream;

	// NOT IMPLEMENTED
	ConsoleOut(const ConsoleOut&);
	ConsoleOut& operator=(const ConsoleOut&);

public:
	explicit ConsoleOut(Mutex         *consoleLock,
		std::ostream&  stream = std::cout)
		: d_consoleLock(consoleLock)
		, d_stream(stream)
	{}

	~ConsoleOut() {
		MutexGuard guard(d_consoleLock);
		d_stream << d_buffer.str();
		d_stream.flush();
	}

	template <typename T>
	std::ostream& operator<<(const T& value) {
		return d_buffer << value;
	}

	std::ostream& stream() {
		return d_buffer;
	}
};

struct SessionContext
{
	Mutex            d_consoleLock;
	Mutex            d_mutex;
	bool             d_isStopped;
	SubscriptionList d_subscriptions;

	SessionContext()
		: d_isStopped(false)
	{
	}
};

class EMSXEventHandler : public EventHandler
{
	bool                     d_isSlow;
	SubscriptionList         d_pendingSubscriptions;
	std::set<CorrelationId>  d_pendingUnsubscribe;
	SessionContext          *d_context_p;
	Mutex                   *d_consoleLock_p;

	bool processSessionEvent(const Event &event, Session *session)
	{

		ConsoleOut(d_consoleLock_p) << "Processing SESSION_EVENT" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {

			Message msg = msgIter.message();

			if (msg.messageType() == SESSION_STARTED) {
				ConsoleOut(d_consoleLock_p) << "Session started..." << std::endl;
				session->openServiceAsync(d_service.c_str());
			}
			else if (msg.messageType() == SESSION_STARTUP_FAILURE) {
				ConsoleOut(d_consoleLock_p) << "Session startup failed" << std::endl;
				return false;
			}
		}
		return true;
	}

	bool processServiceEvent(const Event &event, Session *session)
	{

		ConsoleOut(d_consoleLock_p) << "Processing SERVICE_EVENT" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {

			Message msg = msgIter.message();

			if (msg.messageType() == SERVICE_OPENED) {
				ConsoleOut(d_consoleLock_p) << "Service opened..." << std::endl;

				Service service = session->getService(d_service.c_str());

				Request request = service.createRequest("CreateOrderAndRouteManually");

				// The fields below are mandatory
				request.set("EMSX_TICKER", "IBM US Equity");
				request.set("EMSX_AMOUNT", 1000);
				request.set("EMSX_ORDER_TYPE", "MKT");
				request.set("EMSX_TIF", "DAY");
				request.set("EMSX_HAND_INSTRUCTION", "ANY");
				request.set("EMSX_SIDE", "BUY");
				request.set("EMSX_BROKER", "BB");

				// The fields below are optional
				//request.set("EMSX_ACCOUNT","TestAccount");
				//request.set("EMSX_CFD_FLAG", "1");
				//request.set("EMSX_CLEARING_ACCOUNT", "ClrAccName");
				//request.set("EMSX_CLEARING_FIRM", "FirmName");
				//request.set("EMSX_EXCHANGE_DESTINATION", "ExchDest");
				//request.set("EMSX_EXEC_INSTRUCTIONS", "AnyInst");
				//request.set("EMSX_GET_WARNINGS", "0");
				//request.set("EMSX_GTD_DATE", "20170105");
				//request.set("EMSX_INVESTOR_ID", "InvID");
				//request.set("EMSX_LIMIT_PRICE", 123.45);
				//request.set("EMSX_LOCATE_BROKER", "BMTB");
				//request.set("EMSX_LOCATE_ID", "SomeID");
				//request.set("EMSX_LOCATE_REQ", "Y");
				//request.set("EMSX_NOTES", "Some notes");
				//request.set("EMSX_ODD_LOT", "0");
				//request.set("EMSX_ORDER_ORIGIN", "");
				//request.set("EMSX_ORDER_REF_ID", "UniqueID");
				//request.set("EMSX_P_A", "P");
				//request.set("EMSX_RELEASE_TIME", 34341);
				//request.set("EMSX_REQUEST_SEQ", 1001);
				//request.set("EMSX_SETTLE_DATE", 20170106);
				//request.set("EMSX_STOP_PRICE", 123.5);

				ConsoleOut(d_consoleLock_p) << "Request: " << request << std::endl;

				requestID = CorrelationId();

				session->sendRequest(request, requestID);

			}
			else if (msg.messageType() == SERVICE_OPEN_FAILURE) {
				ConsoleOut(d_consoleLock_p) << "Error: Service failed to open" << std::endl;
				return false;
			}

		}
		return true;
	}

	bool processResponseEvent(const Event &event, Session *session)
	{
		ConsoleOut(d_consoleLock_p) << "Processing RESPONSE_EVENT" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {

			Message msg = msgIter.message();

			ConsoleOut(d_consoleLock_p) << "MESSAGE: " << msg << std::endl;

			if (msg.messageType() == ERROR_INFO) {

				int errorCode = msg.getElementAsInt32("ERROR_CODE");
				std::string errorMessage = msg.getElementAsString("ERROR_MESSAGE");

				ConsoleOut(d_consoleLock_p) << "ERROR CODE: " << errorCode << "\tERROR MESSAGE: " << errorMessage << std::endl;
			}
			else if (msg.messageType() == CREATE_ORDER_AND_ROUTE_MANUALLY) {

				int emsx_sequence = msg.getElementAsInt32("EMSX_SEQUENCE");
				int emsx_route_id = msg.getElementAsInt32("EMSX_ROUTE_ID");
				std::string message = msg.getElementAsString("MESSAGE");

				ConsoleOut(d_consoleLock_p) << "EMSX_SEQUENCE: " << emsx_sequence << "\tEMSX_ROUTE_ID: " << emsx_route_id << "\tMESSAGE: " << message << std::endl;
			}

		}
		return true;
	}

	bool processMiscEvents(const Event &event)
	{
		ConsoleOut(d_consoleLock_p) << "Processing UNHANDLED event" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {
			Message msg = msgIter.message();
			ConsoleOut(d_consoleLock_p) << msg.messageType().string() << "\n" << msg << std::endl;
		}
		return true;
	}

public:
	EMSXEventHandler(SessionContext *context)
		: d_isSlow(false)
		, d_context_p(context)
		, d_consoleLock_p(&context->d_consoleLock)
	{
	}

	bool processEvent(const Event &event, Session *session)
	{
		try {
			switch (event.eventType()) {
			case Event::SESSION_STATUS: {
				MutexGuard guard(&d_context_p->d_mutex);
				return processSessionEvent(event, session);
			} break;
			case Event::SERVICE_STATUS: {
				MutexGuard guard(&d_context_p->d_mutex);
				return processServiceEvent(event, session);
			} break;
			case Event::RESPONSE: {
				MutexGuard guard(&d_context_p->d_mutex);
				return processResponseEvent(event, session);
			} break;
			default: {
				return processMiscEvents(event);
			}  break;
			}
		}
		catch (Exception &e) {
			ConsoleOut(d_consoleLock_p)
				<< "Library Exception !!!"
				<< e.description() << std::endl;
		}
		return false;
	}
};

class CreateOrderAndRouteManually
{

	SessionOptions            d_sessionOptions;
	Session                  *d_session;
	EMSXEventHandler		 *d_eventHandler;
	SessionContext            d_context;

	bool createSession() {
		ConsoleOut(&d_context.d_consoleLock)
			<< "Connecting to " << d_sessionOptions.serverHost()
			<< ":" << d_sessionOptions.serverPort() << std::endl;

		d_eventHandler = new EMSXEventHandler(&d_context);
		d_session = new Session(d_sessionOptions, d_eventHandler);

		d_session->startAsync();

		return true;
	}

public:

	CreateOrderAndRouteManually()
		: d_session(0)
		, d_eventHandler(0)
	{


		d_sessionOptions.setServerHost("localhost");
		d_sessionOptions.setServerPort(8194);
		d_sessionOptions.setMaxEventQueueSize(10000);
	}

	~CreateOrderAndRouteManually()
	{
		if (d_session) delete d_session;
		if (d_eventHandler) delete d_eventHandler;
	}

	void run(int argc, char **argv)
	{
		if (!createSession()) return;

		// wait for enter key to exit application
		ConsoleOut(&d_context.d_consoleLock)
			<< "\nPress ENTER to quit" << std::endl;
		char dummy[2];
		std::cin.getline(dummy, 2);
		{
			MutexGuard guard(&d_context.d_mutex);
			d_context.d_isStopped = true;
		}
		d_session->stop();
		ConsoleOut(&d_context.d_consoleLock) << "\nExiting..." << std::endl;
	}
};

int main(int argc, char **argv)
{
	std::cout << "Bloomberg - EMSX API Example - CreateOrderAndRouteManually" << std::endl;
	CreateOrderAndRouteManually createOrderAndRouteManually;
	try {
		createOrderAndRouteManually.run(argc, argv);
	}
	catch (Exception &e) {
		std::cout << "Library Exception!!!" << e.description() << std::endl;
	}
	// wait for enter key to exit application
	std::cout << "Press ENTER to quit" << std::endl;
	char dummy[2];
	std::cin.getline(dummy, 2);
	return 0;
}
