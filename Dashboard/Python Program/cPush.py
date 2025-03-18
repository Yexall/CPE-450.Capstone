# || Developer's Note ||
# This program assumes that an .xml file (with machine metrics) is stored in the same folder as this Python code.
# You run the program using the command: python ./cPush.py
#
# The purpose of this program is to:
# 1. Read an XML file containing machine metrics (from an MTConnect stream).
# 2. Extract specific data (such as spindle status, cycle times, emergency stop, etc.).
# 3. Format the extracted data so that Prometheus can understand it.
# 4. Send (or "push") these formatted metrics to a Prometheus server.

##### ----- Imports ----- #####
import time
import requests                     # Used to send HTTP requests (to push data to Prometheus)
import xml.etree.ElementTree as ET  # Used to parse and work with XML files

##### ----- Macros (Constants) ----- #####
JOB_NAME = "Machine Metrics"        # This is the name of the Prometheus job that will receive the metrics.
MACHINE_URL = "http://192.168.10.10:8082/UMC-500SS/current" # The URL to request live XML data from the machine via MTConnect.
PROMETHEUS_PUSH_URL = (f"http://localhost:9091/metrics/job/{JOB_NAME}") # The URL for Prometheus push gateway.

# -------------------------------------------------------------------------------- #
# Purpose: Sends the collected metric data to the Prometheus push gateway.
# Parameters:
#   - metricArray: A list of formatted metric strings to send.
#
# If there are metrics in the list, the function joins them into a single string,
# sends them via a POST request to the Prometheus server, and prints a success or error message.
def pushMetric(metricArray):
    if metricArray:  # Check if there is any metric to push.
        # Join all metric strings with newline characters and send as POST request.
        response = requests.post(PROMETHEUS_PUSH_URL,
                                 data="\n".join(metricArray) + "\n",
                                 headers={"Content-Type": "text/plain"})
        
        # Check the HTTP response status.
        if response.status_code == 200:
            print(f"Successfully pushed {len(metricArray)} metric to Prometheus.\n")
        else:
            print(f"Failed to push metricArray! HTTP {response.status_code}: {response.text}")
    else:
        print("No valid data items found.")

# -------------------------------------------------------------------------------- #
# Purpose: Formats a single metric so that Prometheus can understand it.
# Parameters:
#   - metricArray: The list that will hold all formatted metric strings.
#   - deviceName: The name of the machine (extracted from the XML).
#   - metricName: The name of the metric (for example, SpindleEnabled or RunStatus).
#   - itemID: The unique identifier for the metric from the XML.
#   - value: The actual value extracted from the XML.
#   - dataType: A type that tells us how to translate the value (e.g., boolean statuses).
#
# This function uses a Python match-case statement (similar to a switch-case)
# to convert specific string values into numeric values that Prometheus requires.
def formatMetric(metricArray, deviceName, metricName, itemID, value, dataType):
    translatedValue = -1  # Default numeric value (Indicates an error if not updated)

    # Checks what kind of data we have and translates it accordingly.
    match dataType:
        case "SpindleStatus":
            # If the spindle status is "true", we set it to 1 (active); otherwise, 0.
            if value != "true":
                translatedValue = 0
            else:
                translatedValue = 1
        case "EmergencyStatus":
            # If the emergency stop status is "ARMED", we set it to 1.
            if value == "TRIGGERED":
                translatedValue = 0
            elif value == "ARMED":
                translatedValue = 1
        case "EventLogStatus":
            # translatedValue is based off the id number of the corresponding values in the XML 
            if value == "EVENT LOG IS EMPTY":
                translatedValue = 0
            elif value == "Program Started":
                translatedValue = 1
            elif value == "Program Stopped":
                translatedValue = 2
            elif value == "Feed Override":
                translatedValue = 3
            elif value == "Spindle Override":
                translatedValue = 4
            elif value == "Rapid Override":
                translatedValue = 5
            elif value == "Feedhold":
                translatedValue = 6
        case "AlarmStatus":
            # If there are no active alarms, we set it to 0.
            if value == "NO ACTIVE ALARMS":
                translatedValue = 0
        case "RunStatus":
            # If the run status is "ACTIVE", we set it to 1.
            if value == "ACTIVE":
                translatedValue = 1
            elif value == "STOPPED":
                translatedValue = 2
            elif value == "FEED_HOLD":
                translatedValue = 6
        case "MachineStatus":
            # If the machine status is "AVAILABLE", we set it to 1.
            if value == "AVAILABLE":
                translatedValue = 1
        case None:
            # If no special data type is provided, we use the original value.
            translatedValue = value
        case _:
            # For any unexpected data type, print a message.
            print(f"New data type encountered: {dataType}")

    # Create a formatted metric string using Prometheus's text format.
    # It includes labels for the machine model and data item ID.
    metric = (f'{metricName}{{MachineModel="{deviceName}", DataItemId="{itemID}"}} {translatedValue}')
    print(metric)
    metricArray.append(metric)  # Add this metric to the list.

# -------------------------------------------------------------------------------- #
# Purpose: Parses the XML file to extract machine metrics and pushes them to Prometheus.
#
# Step-by-step:
#   1. Parse the XML file and get its root element.
#   2. Define the XML namespace (needed because the XML uses namespaces).
#   3. Create an empty list to hold all metric strings.
#   4. Find the <Streams> element in the XML.
#   5. Within <Streams>, locate the <DeviceStream> element (which represents the machine).
#   6. For each <ComponentStream> (a section of metrics), check its name and extract the relevant messages.
#   7. Format each extracted metric using formatMetric and add it to the list.
#   8. Finally, push all formatted metrics to Prometheus.
def fetchData():
    response = requests.get(MACHINE_URL)  # Request live XML data from MTConnect
    if response.status_code == 200:
        # Parse the XML content and obtain the root element.
        # The 'root' holds the main element of the XML document with all machine metrics.
        root = ET.fromstring(response.content)

        # Define the namespace used in the XML file.
        namespace = {"m": "urn:mtconnect.org:MTConnectStreams:1.2"}

        # Initialize an empty list to store the formatted metric strings.
        metricArray = []

        # Find the <Streams> element (the main container for all data streams).
        streams = root.find("m:Streams", namespace)
        if streams is not None:
            # Find the <DeviceStream> element, which represents the machine.
            deviceStream = streams.find("m:DeviceStream", namespace)

            if deviceStream is not None:
                # Get the machine's name from the DeviceStream element.
                deviceName = deviceStream.get("name")

                # Loop through each <ComponentStream> element (each section of data).
                for componentStream in deviceStream.findall("m:ComponentStream", namespace):
                    # Get the name of the component (e.g., Spindles, MachineController, etc.)
                    componentName = componentStream.get("name")

                    # ------------------------------------------------------ #
                    # Section for "Spindles": extracting spindle-related data.
                    if componentName == "Spindles":
                        # Find the <Events> element that contains messages for the spindles.
                        events = componentStream.find("m:Events", namespace)

                        if events is not None:
                            # Loop through each <Message> in the Spindles section.
                            for message in events.findall("m:Message", namespace):
                                name = message.get("name")
                                itemID = message.get("dataItemId")
                                value = message.text

                                # Check if this message is a SpindleTime message.
                                if name == "SpindleTime":
                                    formatMetric(metricArray, deviceName, name, itemID, value, None)

                                # If the message is "SpindleEnabled", format it as a boolean (SpindleStatus).
                                if name == "SpindleEnabled":
                                    dataType = "SpindleStatus"
                                    formatMetric(metricArray, deviceName, name, itemID, value, dataType)

                    # ------------------------------------------------------ #
                    # Section for "MachineController": extracting controller-related data.
                    if componentName == "MachineController":
                        # Find both <Samples> and <Events> sections under MachineController.
                        samples = componentStream.find("m:Samples", namespace)
                        events = componentStream.find("m:Events", namespace)

                        if samples is not None:
                            # Process accumulated time metrics (like LastCycle, ThisCycle, CycleRemainingTime).
                            for accumulatedTime in samples.findall("m:AccumulatedTime", namespace):
                                name = accumulatedTime.get("name")
                                itemID = accumulatedTime.get("dataItemId")
                                value = accumulatedTime.text

                                if name == "LastCycle":
                                    formatMetric(metricArray, deviceName, name, itemID, value, None)
                                if name == "ThisCycle":
                                    formatMetric(metricArray, deviceName, name, itemID, value, None)
                                if name == "CycleRemainingTime":
                                    formatMetric(metricArray, deviceName, name, itemID, value, None)
                            
                            # Process spindle speed metrics.
                            for spindleSpeed in samples.findall("m:SpindleSpeed", namespace):
                                name = spindleSpeed.get("name")
                                itemID = spindleSpeed.get("dataItemId")
                                value = spindleSpeed.text

                                if name == "SpindleSpeed":
                                    formatMetric(metricArray, deviceName, name, itemID, value, None)
                        
                        if events is not None:
                            # Process emergency stop events.
                            for emergencyStop in events.findall("m:EmergencyStop", namespace):
                                name = emergencyStop.get("name")
                                itemID = emergencyStop.get("dataItemId")
                                value = emergencyStop.text

                                if name == "EmergencyStop":
                                    dataType = "EmergencyStatus"
                                    formatMetric(metricArray, deviceName, name, itemID, value, dataType)
                            
                            # Process event log and active alarms.
                            for message in events.findall("m:Message", namespace):
                                name = message.get("name")
                                itemID = message.get("dataItemId")
                                value = message.text

                                # Check if this message is a MachineRunTime message.
                                if name == "MachineRunTime":
                                    formatMetric(metricArray, deviceName, name, itemID, value, None)

                                # Check if this message is an EventLog message.
                                if name == "EventLog":
                                    dataType = "EventLogStatus"  # Set the data type for event logs.

                                    # If the event log is empty, simply push that metric.
                                    if value == "EVENT LOG IS EMPTY":
                                        formatMetric(metricArray, deviceName, name, itemID, value, dataType)
                                    else:   # If the event log is not empty, iterate over the nested elements.
                                        for haas in message.findall("m:Haas", namespace):
                                            for eventLogEntries in haas.findall("m:EventLogEntries", namespace):
                                                i = 1  # Initialize a counter to make each metric name unique.
                                                
                                                for eventLogEntry in eventLogEntries.findall("m:EventLogEntry", namespace):
                                                    value = eventLogEntry.text
                                                    # Use a unique metric name by appending the counter (e.g., EventLog_1, EventLog_2, etc.).
                                                    formatMetric(metricArray, deviceName, f"{name}_{i}", itemID, value, dataType)
                                                    i = i + 1
                            
                            # Process run status events.
                            for execution in events.findall("m:Execution", namespace):
                                name = execution.get("name")
                                itemID = execution.get("dataItemId")
                                value = execution.text

                                if name == "RunStatus":
                                    dataType = "RunStatus"
                                    formatMetric(metricArray, deviceName, name, itemID, value, dataType)
                    
                    # ------------------------------------------------------ #
                    # Section for "UMC-500SS": extracting machine status (availability).
                    if componentName == "UMC-500SS":
                        events = componentStream.find("m:Events", namespace)

                        if events is not None:
                            for availability in events.findall("m:Availability", namespace):
                                name = availability.get("name")
                                itemID = availability.get("dataItemId")
                                value = availability.text

                                if name == "Availability":
                                    dataType = "MachineStatus"
                                    formatMetric(metricArray, deviceName, name, itemID, value, dataType)

            # After processing all components, push the collected metrics to Prometheus.
            pushMetric(metricArray)

# -------------------------------------------------------------------------------- #
# Purpose: Start the data fetching and metric pushing process. Repeats every 5 seconds
while (1):
    fetchData()
    time.sleep(1) # Sleep for 5 seconds
    
