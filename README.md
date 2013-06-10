Bank
====

Statsd and Metricsd frontend for UDP packets aggregation

Concept
-------

Bank is an aggregator for UDP packets sent to [Metricsd][metricsd] and [Statsd][statsd].

Without an aggregator, UDP packets are sent from applications to Statsd/Metricsd directly:

       Application 
    => Statsd/Metricsd 
    => Graphite

With aggregators, UDP packets are aggregated at various levels before reaching Statsd/Metricsd:

       Application 
    => Local aggregator (Bank) 
    => Other layers of Aggregators (Banks) 
    => Statsd/Metricsd 
    => Graphite

The second approach aims at reducing the number of UDP packages transmitted by the network, and increasing the percentage of UDP data carried in each packet.  

**Aggregation by increasing packet size**

* Bank combine data carried from multiple UDP packets and send them by one packet. This kind of aggregation works best when Bank and the sender are placed in the same machine.

           "api_success:10|c" + "api_queue_size:30|g" + "api_resp_time:50|ms” 
        => "api_success:10|c\napi_queue_size:30|g\napi_resp_time:50|ms"

**Aggregation by doing math**

* Many packets of "Count" data type are aggregated into one packet with their values summed:

        "api_success:10|c" + "api_success:90|c" => "api_success:100|c"

* Many packets of "Gauge" data type are aggregated into one packet with the one latest value:

        "api_queue_size:30|g" + "api_queue_size:25|g" => "hulu_api_queue_size:25|g"

* Data with sample rate are sampled locally:
        
        "api_sample:100|g|@0.1" => accepted as "api_sample:100|g” with 0.1 probability

**Other Features**

* Send aggregated packet when one of the three criteria is met: 
    * A configurable max packet length is reached
    * A configurable number of packets have been aggregated
    * A configurable time interval has elapsed
* Can be used as the frontend of both Statsd and Metricsd because it respects both protocols.
* Respects "delete". 

        "hulu_api:delete|h" => discard current histogram data of hulu_api
                            => send "hulu_api:delete|h" to downstream.
* Can parse packets that are already aggregated
* Consistent hashing for downstream


Installation
------------
 1. git clone bank repo
 2. cd bank && make

Configuration
-------------
An example conf file **bank.conf** can be found in this repo

**Section bank**

* port:               
  * The listening port of bank
  * Default to 8125, the Statsd default port number
* send-interval:
  * The maximum waiting period to send UDP package(s) to metricsd/statsd
  * Default to 0.5 (seconds)
* queue-limit:
  * The maximum number of data points to store in bank before sending the next packet to downstream
  * Default to 50
* max-message-length: 
  * The max length of a message
  * It should be less than or equal to (MTU_length - header_length) to avoid fragmentation. Testing is recommended.
  * Default to 800
* pid-file:
  * The pid file name
  * Default to bank.pid
* health-check:
  * Whether to run the health-check server, which is a simple TCP server responding "ok".
  * Default to False

**Section destiny**

* hosts:
  * Comma separated downstreams. In the format of ip:port
  * If no port is specified it defaults to 8125, the Statsd default port
  * Requires at least host
* health-check:
  * Whether health_check downstream
  * It tries to do TCP connection and send a configurable message
  * If the connection fails, that downstream endpoint is marked down, and no traffic will be sent
  * Default to False
* health-check-interval:
  * Downstream health_check interval in seconds
  * Default to 1 seconds
* health-check-msg:
  * Downstream TCP health_check message
  * Default to "health", the Statsd protocol
* consistent-hashing:
  * Use consistent hashing for downstream
  * It should be used with heath-check=True
  * Default to False
* consistent-hashing-replica:
  * The replica number for consistent hash ring
  * Theoretically a larger number is better for uniformly distribute endpoints
  * Default to 100

**Section logging**

* error-log:  
  * Error log file name
  * Default to bank.log
* log-level:  
  * Error log level. Can be INFO, DEBUG, WARNING, ERROR, CRITICAL
  * Default to DEBUG

Run
---
*./bank -c {configuration-file}*
Without the configuration file Bank looks for bank.conf in current directory.

Licence
-------

Copyright (C) 2013 by Hulu, LLC

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

[metricsd]: https://github.com/mojodna/metricsd
[statsd]: https://github.com/etsy/statsd/