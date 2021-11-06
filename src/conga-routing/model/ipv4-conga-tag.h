/* Conga packets carry the following state:
 * - LBTag: partially identifies the packet's path
 * - CE: used by switches along the packet's path to convey the extent of
 *       congestion
 * - FB_LBTag and FB_Metric: used by destination leaves to piggyback
 *                           congestion information back to the source leaves.
 *                           FB_LBTag indicates which LBTag the feedback is
 *                           for.
 *                           FB_Metric provides the associated congestion
 *                           metric.
*/

#ifndef NS3_IPV4_CONGA_TAG
#define NS3_IPV4_CONGA_TAG

#include "ns3/tag.h"

namespace ns3 {

class Ipv4CongaTag: public Tag {
public:
  // constructor
  Ipv4CongaTag();

  static TypeId GetTypeId();

  // setter and getter for load balancing tag
  void SetLbTag(uint32_t lbTag);
  uint32_t GetLbTag() const;

  // setter and getter for CE field
  void SetCe(uint32_t ce);
  uint32_t GetCe() const;

  // setter and getter for FB_LBTag
  void SetFbLbTag(uint32_t fbLbTag);
  uint32_t GetFbLbTag() const;

  // setter and getter for FB_Metric
  void SetFbMetric(uint32_t fbMetric);
  uint32_t GetFbMetric() const;

  virtual TypeId GetInstanceTypeId() const;
  virtual uint32_t GetSerializedSize() const;
  virtual void Serialize(TagBuffer i) const;
  virtual void Deserialize(TagBuffer i);
  virtual void Print(std::ostream &os) const;

private:
  // the 4 packet fields
  uint32_t m_lbTag;
  uint32_t m_ce;
  uint32_t m_fbLbTag;
  uint32_t m_fbMetric;
};

}

#endif
