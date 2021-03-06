#include<bits/stdc++.h>
using namespace std;
/*----------------------------------------------------------------------*/

class source
{
public:
	int window_size, id, highest_unacked;
	double timeout;
	static int counter;

	source() {
		this->window_size = 1;
		this->id = ++counter;
		this->timeout = 8.0;
		this->highest_unacked = -1;
	}
};

class event
{
public:
	/*
		event_type
		0 - (Packet generated by source).
		1 - (Packet started moving from source to switch). 
		2 - (packet reached input port of switch).
		3 - (packet reached the output port of switch).
		4 - (Packet reached the destination sink, Ack generated by the sink for the source).
		5 - (Ack starts to move towards the switch).
		6 - (Ack reaches the output port of the switch).
		7 - (Source checks for timeout of the packet).
		8 - (Ack reaches the source).
	*/
	int package_id, event_type, source_id;
	double timestamp;

	/*
		Events will be sorted by timestamp and then by package_id in the priority queue.
	*/
	event(double timestamp, int package_id, int event_type, int source_id) {
		this->package_id = package_id;
		this->timestamp = timestamp;
		this->event_type = event_type;
		this->source_id = source_id;
	}
};

bool cmp(event a, event other_event) { 
	if(a.timestamp != other_event.timestamp) {
		return a.timestamp > other_event.timestamp;
	}
	return (a.package_id > other_event.package_id);
}

class link
{
public:
	int id, bandwidth;
	double source_to_switch_busytill, switch_to_source_busytill;
	static int counter;

	link(int bandwidth) {
		this->bandwidth = bandwidth;
		this->id = ++counter;
		this->source_to_switch_busytill = 0.0;
		this->switch_to_source_busytill = 0.0;
	}
};

class packet
{
public:
	int id, source_id; double generation_timestamp;
	static int counter;
	packet(int id, int source_id, double generation_timestamp) {
		this->id = id;
		this->source_id = source_id;
		this->generation_timestamp = generation_timestamp;
	}
};

class network_switch
{
public:
	double busytill;
	int outgoing_queue_size, CAPACITY=500;
	network_switch() {
		this->busytill = 0.0;
		this->outgoing_queue_size = 0;
	}
};

int source::counter=0; int link::counter=0; int packet::counter=0;
int packet_size=5;
priority_queue<event, vector<event>, std::function<bool(event, event)>> pq(cmp); // priority queue of events.
vector<packet> packets;

double get_packet_delay(int bandwidth) {
	return 1.0*packet_size/bandwidth;
}

void additive_increase(vector<source> &sources, int index) {
	sources[index].window_size++;
}

void multiplicative_decrease(vector<source> &sources, int index) {
	sources[index].window_size/=2;
	sources[index].window_size = max(sources[index].window_size, 1);
}

void simulate() {
	source::counter=0; link::counter=0; packet::counter=0;
	packets.clear();
	while(!pq.empty()) pq.pop();

	int limit=100,done=0;
	link switch_to_sink(5);
	vector<source> sources(3);
	vector<link> source_to_switch = {link(5), link(10), link(5)};
	int total_generated_packets = 0;
	vector<double> total_rtt_time(3, 0.0);
	int done_counter[3]={0}, lost_count[3]={0};
	vector<set<int>> unacked_packets(3);

	for (int i = 0; i < 3; ++i)
	{
		// set source id.
		sources[i].id=i;
		
		// Generating the first batch of packets from each of the three sources.
		for (int j = 0; j < sources[i].window_size; ++j)
		{
			event starter(0.0, i+j, 0, sources[i].id);
			pq.push(starter);
			sources[i].highest_unacked = i + j;
		}
	}

	network_switch net_switch;

	while(done < limit) {
		event current_event = pq.top(); pq.pop();
		switch(current_event.event_type) {
			case 0 : {
				/*
					A packet has been generated, schedule it to move to the input port of switch.
				*/
				total_generated_packets++;
				packets.push_back(packet(current_event.package_id, current_event.source_id, current_event.timestamp));
				event source_to_switch_event(current_event.timestamp, current_event.package_id, 1, current_event.source_id);
				pq.push(source_to_switch_event);
				unacked_packets[current_event.source_id].insert(current_event.package_id);

				/*
					Schedule the checkup of timeout.
				*/
				event check_timeout(current_event.timestamp + sources[current_event.source_id].timeout,
					current_event.package_id,
					7,
					current_event.source_id);
				pq.push(check_timeout);
				break;
			}
			case 1: {
				if(source_to_switch[current_event.source_id].source_to_switch_busytill > current_event.timestamp) {
					/*
						Another packet is using this link, so again schedule this event with time when the link is available.
					*/
					event source_to_switch_event(source_to_switch[current_event.source_id].source_to_switch_busytill, current_event.package_id, 1, current_event.source_id);
					pq.push(source_to_switch_event);
				} else {
					/*
						Link is available, so the packet is good to go.
						Update the busytill of the link.
						We also check if the buffer of the switch is full or not.
					*/
					if(net_switch.outgoing_queue_size < net_switch.CAPACITY) {
						event packet_reaches_input_port_switch(
							current_event.timestamp + get_packet_delay(source_to_switch[current_event.source_id].bandwidth), 
							current_event.package_id,
						 	2,
						 	current_event.source_id
						);

						source_to_switch[current_event.source_id].source_to_switch_busytill = current_event.timestamp + 
							get_packet_delay(source_to_switch[current_event.source_id].bandwidth);
						pq.push(packet_reaches_input_port_switch);
						net_switch.outgoing_queue_size++;
					}
				}
				break;
			}
			case 2:
				if(net_switch.busytill > current_event.timestamp) {
					/*
						Another packet is being transferred between the input and the output port.
					*/
					event packet_reaches_input_port_switch(
						net_switch.busytill, 
						current_event.package_id,
					 	2,
					 	current_event.source_id
					);
					pq.push(packet_reaches_input_port_switch);
				} else {
					/*
						Transfer packet from input to output port of the switch.
					*/
					event packet_reaches_output_port_switch(
						current_event.timestamp + 0.0,
						current_event.package_id,
						3,
						current_event.source_id
					);
					pq.push(packet_reaches_output_port_switch);
					net_switch.busytill=packet_reaches_output_port_switch.timestamp;
				}
				break;
			case 3: {
				if(switch_to_sink.source_to_switch_busytill > current_event.timestamp) {
					/*
						Another packet is using this link, so again schedule this event with time when the link is available.
					*/
					event switch_to_sink_event(switch_to_sink.source_to_switch_busytill,
						current_event.package_id,
						3,
						current_event.source_id);
					pq.push(switch_to_sink_event);
				} else {
					/*
						Link is available, so the packet is good to go.
						Update the busytill of the link.
					*/
					event packet_reaches_sink(
						current_event.timestamp + get_packet_delay(switch_to_sink.bandwidth), 
						current_event.package_id,
					 	4,
					 	current_event.source_id
					);
					switch_to_sink.source_to_switch_busytill = current_event.timestamp + get_packet_delay(switch_to_sink.bandwidth);
					pq.push(packet_reaches_sink);
					net_switch.outgoing_queue_size--;
				}
				break;
			} case 4: {
				event sink_to_switch_event(current_event.timestamp, 
					current_event.package_id,
					5,
					current_event.source_id);
				pq.push(sink_to_switch_event);
				break;
			} case 5: {
				if(switch_to_sink.switch_to_source_busytill > current_event.timestamp) {
					/*
						Another packet is using this link, so again schedule this event with time when the link is available.
					*/
					event sink_to_switch_event(switch_to_sink.switch_to_source_busytill, current_event.package_id, 5, current_event.source_id);
					pq.push(sink_to_switch_event);
				} else {
					/*
						Link is available, so the ack is good to go.
						Update the busytill of the link.
						Also check if the buffer of the switch is already full or not.
					*/
					if(net_switch.outgoing_queue_size < net_switch.CAPACITY) {
						event ack_reaches_input_port_switch(
							current_event.timestamp + get_packet_delay(switch_to_sink.bandwidth), 
							current_event.package_id,
						 	6,
						 	current_event.source_id
						);

						switch_to_sink.switch_to_source_busytill = current_event.timestamp + 
							get_packet_delay(switch_to_sink.bandwidth);
						pq.push(ack_reaches_input_port_switch);
					}
				}
				break;
			} case 6: {
				if(source_to_switch[current_event.source_id].switch_to_source_busytill > current_event.timestamp) {
					/*
						Another packet is using this link, so again schedule this event with time when the link is available.
					*/
					event ack_starts_moving_towards_source(source_to_switch[current_event.source_id].switch_to_source_busytill,
						current_event.package_id,
						6,
						current_event.source_id);
					pq.push(ack_starts_moving_towards_source);
				} else {
					/*
						Link is available, so the ack is good to go.
						Update the busytill of the link.
					*/
					event ack_reaches_the_source(current_event.timestamp + get_packet_delay(source_to_switch[current_event.source_id].bandwidth),
						current_event.package_id,
						8,
						current_event.source_id);
					pq.push(ack_reaches_the_source);
					net_switch.outgoing_queue_size--;
				}
				break;
			} case 7: {
				if(unacked_packets[current_event.source_id].find(current_event.package_id) != unacked_packets[current_event.source_id].end()) {
					/*
						We haven't received the ack in the alloted time. Hence resend it.
					*/
					cout<<"Packet id: "<<current_event.package_id
						<<" dropped. Resending it." << endl;
					// cout<<"Changing generation_timestamp of "<<current_event.package_id<<" to "<<current_event.timestamp<<endl;
					multiplicative_decrease(sources, current_event.source_id);
					lost_count[current_event.source_id]++;
					packets[current_event.package_id].generation_timestamp = current_event.timestamp;
					event source_to_switch_event(current_event.timestamp, current_event.package_id, 1, current_event.source_id);
					pq.push(source_to_switch_event);
				}
				break;
			}
			default: {
				done++;
				cout<<"Packet id: "<<current_event.package_id
					<<", Source_id: "<<current_event.source_id
					<<", Start time: "<<packets[current_event.package_id].generation_timestamp
					<<", End time: "<<current_event.timestamp<<endl;
			
				done_counter[current_event.source_id]++;
				total_rtt_time[current_event.source_id] += (current_event.timestamp-packets[current_event.package_id-1].generation_timestamp);
				auto itr = unacked_packets[current_event.source_id].find(current_event.package_id);
				if(itr != unacked_packets[current_event.source_id].end()) {
					unacked_packets[current_event.source_id].erase(itr);
				}

				if(unacked_packets[current_event.source_id].empty()
			 		|| *unacked_packets[current_event.source_id].begin() > sources[current_event.source_id].highest_unacked) {

					additive_increase(sources, current_event.source_id);
					
					// generate the next set of packets.
					for (int i = 0; i < sources[current_event.source_id].window_size; ++i)
					{
						// printf("Generating next set of packets for %d at time %f\n", current_event.source_id, current_event.timestamp);
						event starter(current_event.timestamp, total_generated_packets++, 0, sources[i].id);
						pq.push(starter);
						sources[i].highest_unacked = total_generated_packets;
					}
				}
			}
		}
	}

}

int main() {
	simulate();
    return 0;
}/*

*/
