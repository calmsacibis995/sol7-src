## THIS FILE WAS GENERATED BY MIB2SCHEMA
## For attributes containing printable ASCII
## characters, you may add a format string
## specification in the attribute characteristics
## line
## As described in RFC 1157, some agents cannot
## accept messages whose length exceeds 484 octets
## For those groups whose var-bind list has many
## entries, you may split them up into different
## var-bind lists
proxy DMTFDEVELOPERS-SP-MIB
description "DMTFDEVELOPERS-SP-MIB agent"
serial 2
rpcid 100122
(

## Enumerated Type Definitions


## Group and Table Declarations

	table	tComponentid
	description "This group defines attributes common to all components.Th"
	characteristics "-K ONE"
	(
		readonly string[128]	a1Manufacturer
		description 	"The name of the manufacturer that produces this component."
		characteristics "-N a1Manufacturer -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.1 -T STRING -A RO -X equal -F 0"

		readonly string[128]	a1Product
		description 	"The name of the component."
		characteristics "-N a1Product -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.2 -T STRING -A RO -X equal -F 0"

		readonly string[128]	a1Version
		description 	"The version for the component."
		characteristics "-N a1Version -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.3 -T STRING -A RO -X equal -F 0"

		readonly string[128]	a1SerialNumber
		description 	"The serial number for this instance of this component."
		characteristics "-N a1SerialNumber -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.4 -T STRING -A RO -X equal -F 0"

		readonly octet[128]	a1Installation
		description 	"The time and date of the last install of this component."
		characteristics "-N a1Installation -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.5 -T STRING -A RO -X equal -F 0"

		readonly int	a1Verify
		description 	"A code that provides a level of verfication that the component
is still installed and working."
		characteristics "-N a1Verify -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.6 -T INTEGER -A RO -X equal -F 0"

		readonly int	a1Componentid
		description 	"The trap info. of the new installed component."
		characteristics "-N a1Componentid -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.7 -T INTEGER -A RO -X equal -F 0"

		readonly string[128]	a1Componentname
		description 	"The trap info for Component name."
		characteristics "-N a1Componentname -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.8 -T STRING -A RO -X equal -F 0"

		readonly string[128]	a1Componentdesc
		description 	"The trap info for Component Desc."
		characteristics "-N a1Componentdesc -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.9 -T STRING -A RO -X equal -F 0"

		readonly int	a1Groupid
		description 	"The trap info for Group Id."
		characteristics "-N a1Groupid -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.10 -T INTEGER -A RO -X equal -F 0"

		readonly string[128]	a1Groupname
		description 	"The trap info for Group Name."
		characteristics "-N a1Groupname -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.11 -T STRING -A RO -X equal -F 0"

		readonly string[128]	a1Languagename
		description 	"The trap info for Language Name."
		characteristics "-N a1Languagename -O 1.3.6.1.4.1.42.2000.1.1.1.1.1.12 -T STRING -A RO -X equal -F 0"

	)

	table	tSpIndicationSubscription
	description "This group defines the subscription information for a "
	characteristics "-K ONE"
	(
		readwrite string[128]	a2SubscriberRpcType
		description 	"This is an identifier of the type of RPC in useby"
		characteristics "-N a2SubscriberRpcType -O 1.3.6.1.4.1.42.2000.1.1.1.2.1.1 -T STRING -A RW -X equal -F 0"

		readwrite string[128]	a2SubscriberTransportTypesubscriberTrans
		description 	"This is an identifier of the type of Transport inu"
		characteristics "-N a2SubscriberTransportTypesubscriberTrans -O 1.3.6.1.4.1.42.2000.1.1.1.2.1.2 -T STRING -A RW -X equal -F 0"

		readwrite string[128]	a2SubscriberAddressing
		description 	"Addressing information of the managing node thath"
		characteristics "-N a2SubscriberAddressing -O 1.3.6.1.4.1.42.2000.1.1.1.2.1.3 -T STRING -A RW -X equal -F 0"

		readonly int	a2SubscriberIdsubscriberId
		description 	"An ID or handle passed by the managing node to theS"
		characteristics "-N a2SubscriberIdsubscriberId -O 1.3.6.1.4.1.42.2000.1.1.1.2.1.4 -T INTEGER -A RO -X equal -F 0"

		readonly octet[128]	a2SubscriptionExpirationWarningDateStamp
		description 	"On this date and time, the DMI Service Providerwi"
		characteristics "-N a2SubscriptionExpirationWarningDateStamp -O 1.3.6.1.4.1.42.2000.1.1.1.2.1.5 -T STRING -A RO -X equal -F 0"

		readonly octet[128]	a2SubscriptionExpirationDatestamp
		description 	"On this date, after having issued the appropriaten"
		characteristics "-N a2SubscriptionExpirationDatestamp -O 1.3.6.1.4.1.42.2000.1.1.1.2.1.6 -T STRING -A RO -X equal -F 0"

		readonly int	a2IndicationFailureThreshold
		description 	"This is a number that corresponds to the number ofi"
		characteristics "-N a2IndicationFailureThreshold -O 1.3.6.1.4.1.42.2000.1.1.1.2.1.7 -T INTEGER -A RO -X equal -F 0"

	)

	table	tSpFilterInformation
	description "This group defines a row in a table of event filters.O"
	characteristics "-K ???"
	(
		readwrite string[128]	a3SubscriberRpcType
		description 	"This is an identifier of the type of RPC in useby"
		characteristics "-N a3SubscriberRpcType -O 1.3.6.1.4.1.42.2000.1.1.1.3.1.1 -T STRING -A RW -X equal -F 0"

		readwrite string[128]	a3SubscriberTransportTypesubscriberTrans
		description 	"This is an identifier of the type of Transport inu"
		characteristics "-N a3SubscriberTransportTypesubscriberTrans -O 1.3.6.1.4.1.42.2000.1.1.1.3.1.2 -T STRING -A RW -X equal -F 0"

		readwrite string[128]	a3SubscriberAddressing
		description 	"Addressing information of the managing node thath"
		characteristics "-N a3SubscriberAddressing -O 1.3.6.1.4.1.42.2000.1.1.1.3.1.3 -T STRING -A RW -X equal -F 0"

		readonly int	a3SubscriberIdsubscriberId
		description 	"An ID or handle passed by the managing node to theS"
		characteristics "-N a3SubscriberIdsubscriberId -O 1.3.6.1.4.1.42.2000.1.1.1.3.1.4 -T INTEGER -A RO -X equal -F 0"

		readwrite int	a3ComponentId
		description 	"The component ID, as assigned by the DMI ServiceP"
		characteristics "-N a3ComponentId -O 1.3.6.1.4.1.42.2000.1.1.1.3.1.5 -T INTEGER -A RW -X equal -F 0"

		readwrite string[128]	a3GroupClassString
		description 	"The Class string corresponding to the groupsw"
		characteristics "-N a3GroupClassString -O 1.3.6.1.4.1.42.2000.1.1.1.3.1.6 -T STRING -A RW -X equal -F 0"

		readwrite int	a3EventSeverity
		description 	"The event severityevent severity level, at whicha"
		characteristics "-N a3EventSeverity -O 1.3.6.1.4.1.42.2000.1.1.1.3.1.7 -T INTEGER -A RW -X equal -F 0"

	)

agenterrors (
	1	"cannot dispatch request"
	2	"select(2) failed"
	3	"sendto(2) failed"
	4	"recvfrom(2) failed"
	5	"no response from system"
	6	"response too big"
	7	"missing attribute"
	8	"bad attribute type"
	9	"cannot get sysUpTime"
	10	"sysUpTime type bad"
	11	"unknown SNMP error"
	12	"bad variable value"
	13	"variable is read only"
	14	"general error"
	15	"cannot make request PDU"
	16	"cannot make request varbind list"
	17	"cannot parse response PDU"
	18	"request ID - response ID mismatch"
	19	"string contains non-displayable characters"
	20	"cannot open schema file"
	21	"cannot parse schema file"
	22	"cannot open host file"
	23	"cannot parse host file"
	24	"attribute unavailable for set operations"
             )
)


## End of Agent Definitions