import xml.etree.ElementTree as ET
from xml.dom import minidom

########### CONFIGS

server_name = 'UDP_server'
server_prefix = '44.49.53.43.53.45.52.56.45.52.5F.31'
number_of_participants = 100
number_of_topics = 2000

########### /CONFIGS

tree = ET.parse('test_00_tool_help.xml')
root = tree.getroot()

servers = ET.SubElement(root,'servers')
server = ET.SubElement(servers,'server')
server.attrib = {'name': 'server', 'profile_name': server_name}

clients = ET.SubElement(root,'clients')
topics_range = range(number_of_topics)
topics_pointer = 0
for index_client in range(number_of_participants):
    client_name = 'client' + str(index_client)
    profile_name = 'UDP_' + client_name + '_server1'
    client = ET.SubElement(clients,'client')
    client.attrib = {'name': client_name, 'profile_name': profile_name}
    # Here another loop to create publishers
    overlap = number_of_topics/number_of_participants
    for index in range(int(overlap)):
        publisher = ET.SubElement(client,'publisher')
        # Forward overlap function
        topic_number = index + (index_client * overlap)
        if (topic_number > number_of_topics):
            topic_number = topic_number - number_of_topics
        topic_name = 'topic' + str(int(topic_number))
        publisher.attrib = {'topic': topic_name}
    # And another one to create subscribers
    for index in range(int(overlap)):
        subscriber = ET.SubElement(client,'subscriber')
        # Backwards overlap function:
        backwards_overlap_index_client = (index_client - 1 - index)
        if (backwards_overlap_index_client < 0):
            backwards_overlap_index_client = backwards_overlap_index_client + number_of_participants
        first_own_publisher_number = index + (backwards_overlap_index_client * overlap)
        topic_number = first_own_publisher_number
        if (topic_number < 0):
            topic_number = topic_number + number_of_topics
        topic_name = 'topic' + str(int(topic_number))
        subscriber.attrib = {'topic': topic_name}

snapshots = ET.SubElement(root,'snapshots')
snapshots.attrib = {'file': './test_70_single_server_many_topics.snapshot~'}
snapshot = ET.SubElement(snapshots,'snapshot')
snapshot.attrib = {'time': '300'}
snapshot.text = 'test_70_single_server_many_topics'

types = ET.SubElement(root,'types')
type = ET.SubElement(types,'type')
struct = ET.SubElement(type,'struct')
struct.attrib = {'name':'HelloWorld'}
member = ET.SubElement(struct,'member')
member.attrib = {'name':'index', 'type':'uint32'}
member = ET.SubElement(struct,'member')
member.attrib = {'name':'message', 'type':'string'}

profiles = ET.SubElement(root,'profiles')
participant = ET.SubElement(profiles,'participant')
participant.attrib = {'profile_name':server_name}
rtps = ET.SubElement(participant,'rtps')
prefix = ET.SubElement(rtps,'prefix')
prefix.text = server_prefix
builtin = ET.SubElement(rtps,'builtin')
discovery_config = ET.SubElement(builtin,'discovery_config')
discoveryProtocol = ET.SubElement(discovery_config,'discoveryProtocol')
discoveryProtocol.text = 'SERVER'
initialAnnouncements = ET.SubElement(discovery_config,'initialAnnouncements')
count = ET.SubElement(initialAnnouncements,'count')
count.text = '0'
leaseAnnouncement = ET.SubElement(discovery_config,'leaseAnnouncement')
leaseAnnouncement.text = 'DURATION_INFINITY'
leaseDuration = ET.SubElement(discovery_config,'leaseDuration')
leaseDuration.text = 'DURATION_INFINITY'
metatrafficUnicastLocatorList = ET.SubElement(builtin,'metatrafficUnicastLocatorList')
locator = ET.SubElement(metatrafficUnicastLocatorList,'locator')
udpv4 = ET.SubElement(locator,'udpv4')
address = ET.SubElement(udpv4,'address')
address.text = '127.0.0.1'
port = ET.SubElement(udpv4,'port')
port.text = '03811'



for index_participant in range(number_of_participants):
    participant = ET.SubElement(profiles,'participant')
    participant_name = 'client' + str(index_participant)
    participant.attrib = {'profile_name': participant_name}
    rtps = ET.SubElement(participant,'rtps')
    prefix = ET.SubElement(rtps,'prefix')
    hex_client_number = "{:02x}".format(index_participant)
    client_prefix = '63.6c.69.65.6e.74.' + hex_client_number + '.5f.73.31.5f.5f'
    prefix.text = client_prefix
    builtin = ET.SubElement(rtps,'builtin')
    discovery_config = ET.SubElement(builtin,'discovery_config')
    discoveryProtocol = ET.SubElement(discovery_config,'discoveryProtocol')
    discoveryProtocol.text = 'CLIENT'
    discoveryServersList = ET.SubElement(discovery_config,'discoveryServersList')
    RemoteServer = ET.SubElement(discoveryServersList,'RemoteServer')
    RemoteServer.attrib = {'prefix': server_prefix}
    metatrafficUnicastLocatorList = ET.SubElement(RemoteServer,'metatrafficUnicastLocatorList')
    locator = ET.SubElement(metatrafficUnicastLocatorList,'locator')
    udpv4 = ET.SubElement(locator,'udpv4')
    address = ET.SubElement(udpv4,'address')
    address.text = '127.0.0.1'
    port = ET.SubElement(udpv4,'port')
    port.text = '03811'

for index_topic in range(number_of_topics):
    topic = ET.SubElement(profiles,'topic')
    topic_name = 'topic' + str(index_topic)
    topic.attrib = {'profile_name': topic_name}
    name = ET.SubElement(topic,'name')
    name.text = 'topic_' + str(index_topic)
    dataType = ET.SubElement(topic,'dataType')
    dataType.text = 'HelloWorld'






### DEBUG
xmlstr = minidom.parseString(ET.tostring(types)).toprettyxml(indent="    ")
with open("test_70_single_server_many_topics.xml", "wb") as f:
    f.write(xmlstr.encode('utf-8'))
###


xmlstr = minidom.parseString(ET.tostring(root)).toprettyxml(indent="    ")
with open("test_70_single_server_many_topics.xml", "wb") as f:
    f.write(xmlstr.encode('utf-8'))
#with open("test_70_single_server_many_topics.xml", "w") as f:
    #f.write(xmlstr)
