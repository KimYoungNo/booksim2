#ifndef __INTERFACE_HPP__
#define __INTERFACE_HPP__

#include <map>
#include <memory>
#include <string>
#include <queue>
#include <cstdint>
#include <fstream>

class Network;
class Flit;
class TrafficManager;
class NetworkRequest;
class Stats;
class Router;
class OutputSet;

using tRoutingFunction = void(*)(const Router *,
                                 const Flit *,
                                 int, OutputSet *, bool);

namespace booksim2
{

class Interface
{
public:
  enum class Type
  { READ, WRITE, READ_REPLY, WRITE_REPLY, ANY };

  Interface(const std::string&, const int);
  ~Interface() = default;

  void run();
  uint32_t get_flit_size() { return flit_size; }

  bool is_full(uint32_t nid, uint32_t subnet, uint32_t size) const;
  void push(void* packet, uint32_t subnet, 
            uint64_t addr, int bytes, Type type, int src, int dst);
  bool is_empty(uint32_t nid, uint32_t subnet) const;
  const void* top(uint32_t nid, uint32_t subnet) const;
  void pop(uint32_t nid, uint32_t subnet);

  void Transfer2BoundaryBuffer(uint32_t subnet, uint32_t output);
  void WriteOutBuffer(uint32_t subnet, int output, Flit* flit);
  Flit* GetEjectedFlit(uint32_t subnet, uint32_t nid);

  uint64_t get_cycle() const { return clk; }
  bool print_activity() { return gPrintActivity; }
  void print_stats();

  Stats* GetStats(const std::string &name);
  const int HEADER_SIZE;

  bool gPrintActivity;
  int gK;
  int gN;
  int gC;
  int gNodes;
  bool gTrace;
  std::ofstream *gWatchOut;

  std::map<int, int> *anynet_global_routing_table;

  int gNumVCs;
  int gReadReqBeginVC;
  int gReadReqEndVC;
  int gWriteReqBeginVC;
  int gWriteReqEndVC;
  int gReadReplyBeginVC;
  int gReadReplyEndVC;
  int gWriteReplyBeginVC;
  int gWriteReplyEndVC;

  std::map<std::string, tRoutingFunction> gRoutingFunctionMap;
private:

  class BoundaryBufferItem
  {
  public:
    BoundaryBufferItem(): num_packets(0) {}
    inline uint32_t size(void) const { return buffer.size(); }
    inline bool is_empty() const { return num_packets == 0; }
    void pop();
    const void* top() const;
    void push(void* packet, bool is_tail);
    typedef struct Buffer {
      void* packet;
      bool is_tail;
    } Buffer;
  private:
    std::vector<Buffer> buffer;
    uint32_t num_packets;
  };

  const uint32_t REQUEST_VC = 0;
  const uint32_t RESPONSE_VC = 1;

  typedef std::queue<Flit*> EjectionBufferItem;

  int num_nodes;
  int num_subnets;
  int vcs;
  uint32_t flit_size;
  std::vector<Network*> nets;
  uint32_t input_buffer_capacity;

  uint32_t boundary_buffer_capacity;
  std::vector<std::vector<std::vector<BoundaryBufferItem>>> boundary_buffer;
  uint32_t ejection_buffer_capacity;
  std::vector<std::vector<std::vector<EjectionBufferItem>>> ejection_buffer;

  std::vector<std::vector<std::queue<Flit*>>> ejected_flit_queue;

  std::vector<std::vector<int>> round_robin_turn;

  uint32_t clk;

  TrafficManager *traffic_manager;

  void initParameters();
};

} // namespace booksim

#endif