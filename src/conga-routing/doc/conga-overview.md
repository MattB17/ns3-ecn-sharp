# CONGA Overview
CONGA argues that datacenter fabric load balancing is best done in the network, and requires global schemes to handle asymmetry.

CONGA leverages
* a leaf-to-leaf feedback mechanism to convey real-time path congestion metrics to the leaf switches
* flowlet switching to balance load at a finer granularity than flows

### Design
The source leaf makes load balancing decisions based on per uplink congestion metrics
* this is derived by taking the maximum of the local congestion at the uplink and the remote congestion for the path(s) to the destination leaf that originates at the uplink
* the remote metrics are stored in the Congestion-To-Leaf Table on a per destination leaf, per uplink basis and convey the maximum congestion for all the links along the path
* the remote metrics are obtained via feedback from the destination leaf switch, which piggybacks values in its Congestion-From-Leaf Table back to the source leaf
* CONGA measures congestion using the Discounting Rate Estimator (DRE)

Load balancing decisions are made on the first packet of each flowlet
* the source leaf uses the Flowlet Table to keep track of active flowlets and their chosen uplinks

CONGA uses the VXLAN encapsulation format for the overlay to carry the following state
* `LBTag` (4 bits)
  * partially identifies the packet's path
  * it is set by the source leaf to the (switch-local) port number of the uplink the packet is sent on
  * it is used by the destination leaf to aggregate congestion metrics before they are fed back to the source
* `CE` (3 bits)
  * used by switches along the packet's path to convey the extent of congestion
* `FB_LBTag` (4 bits) and `FB_Metric` (3 bits)
  * used by destination leaves to piggyback congestion information back to the source leaves
  * `FB_LBTag` indicates the `LBTag` the feedback is for
  * `FB_Metric` provides its associated congestion metric

The Discounting Rate Estimator (DRE) is for measuring the load of a link
* it maintains a register `X`, which is incremented for each packet sent over the link by the packet size in bytes, and is decremented periodically (every `T_dre`)  with a multiplicative factor `α` between 0 and 1
  * `X <- X(1 - α)`
  * `X` is proportional to the rate of traffic over the link
  * if the traffic rate is `R` then `X ≈ Rτ` where `τ = T_dre / α`
* the congestion metric for the link is obtained by quantizing `X/Cτ` to 3 bits
  * `C` is the link speed

CONGA uses a feedback loop between the source and destination leaf switches to populate the remote metrics in the Congestion-To-Leaf Table at each leaf switch
* the source leaf sends the packet to the fabric with the `LBTag` field set to the uplink port taken by the packet
  * it also sets the `CE` field to 0
* the packet is routed through the fabric to the destination leaf
  * as it traverses each link its `CE` field is update if the link's congestion metric (given by the DRE) is larger than the current value in the packet
* the CE field of the packet received at the destination leaf gives the maximum link congestion along the packet's path
  * this needs to be fed back to the source leaf
  * but since a packet may not be immediately available in the reverse direction, the destination leaf stores the metric in the Congestion-From-Leaf Table (on a per source leaf, per `LBTag` basis) while it waits for an opportunity to piggyback the feedback
* when a packet is sent in the reverse direction, one metric from the Congestion-From-Leaf Table is inserted in its `FB_LBTag` and `FB_Metric` fields for the source leaf
  * the metric is chosen in round-robin fashion
  * as an optimization, it favours metrics whose values have changed since the last time they were fed back
* the source leaf parses the feedback in the reverse packet and updates the Congestion-To-Leaf Table

Every packet simultaneously carries both a metric for its forward path and a feedback metric

CONGA does not have explicit feedback packets
* metrics may become stale if there is insufficient traffic for piggybacking
* to handle this, an aging mechanism is added where a metric that has not been updated for a long time gradually decays to zero
* this also guarantees that a path that appears to be congested is eventually probed again

Flowlets are detected and tracked in the leaf switches using the FLowlet Table
* each entry of the table consists of a port number, a valid bit, and an age bit
* lookup an entry based on a hash of a packet's 5-tuple
  * if the entry is valid (`valid_bit == 1`), the flowlet is active and the packet is sent on the port indicated in the entry
  * otherwise, the incoming packet starts a new flowlet
    * in this case, a load balancing decision is made and cached in the table for use by subsequent packets
    * the valid bit is set to 1
* flowlet entries time out using the age bit
  * each incoming packet resets the age bit
  * a timer periodically (every `T_fl` seconds) checks the age bit before setting it
  * if the age bit is set when the timer checks it, then there have not been any packets for that entry in the last `T_fl` seconds and the entry expires `valid_bit = 0`)
  * for a new flowlet, the uplink port that minimizes the maximum of the local metric (from the local DREs) and the remote metric (from the Congestion-To-Leaf Table) is chosen
  * if multiple uplinks are equally good, one is chosen at random with preference given to the port cached in the (invalid) entry in the Flowlet Table
    * that is, a flow only moves if there is a strictly better uplink than the one its last flowlet took
