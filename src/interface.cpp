#include <fstream>
#include <sstream>
#include <algorithm>

#include "routefunc.hpp"
#include "booksim_config.hpp"
#include "trafficmanager.hpp"
#include "networks/network.hpp"
#include "interface.hpp"

namespace booksim2
{
Interface::Interface(const std::string& config_file_path,
                     const int num_nodes)
: HEADER_SIZE(0),
  num_nodes(num_nodes),
  clk(0)
{
    BookSimConfig config;
    config.ParseFile(config_file_path);
    initParameters();
  
    this->num_subnets = config.GetInt("subnets");
    assert(num_subnets);

    this->nets.resize(num_subnets);
    InitializeRoutingMap(this, config);

    for (int n=0; n<num_subnets; ++n)
    {
        std::ostringstream name;
        name << "network_" << n;
        this->nets[n] = Network::New(config, name.str(), this);
    }

    this->flit_size = config.GetInt("flit_size");
    this->ejection_buffer_capacity = config.GetInt("ejection_buffer_size") ?
        config.GetInt("ejection_buffer_size") : config.GetInt("vc_buf_size");
    this->input_buffer_capacity = config.GetInt("input_buffer_size") ?
        config.GetInt("input_buffer_size") : 9;
    this->boundary_buffer_capacity = config.GetInt("boundary_buffer_size");
    assert(boundary_buffer_capacity);

    std::string watch_file = config.GetStr("watch_out");
    if ((watch_file=="") || (watch_file=="-"))
        this->gWatchOut = nullptr;
    else
        this->gWatchOut = new std::ofstream(watch_file);

    vcs = config.GetInt("num_vcs");

    this->boundary_buffer.resize(num_subnets);
    this->ejection_buffer.resize(num_subnets);
    this->round_robin_turn.resize(num_subnets);
    this->ejected_flit_queue.resize(num_subnets);

    for (int s=0; s<num_subnets; ++s)
    {
        this->boundary_buffer[s].resize(num_nodes);
        this->ejection_buffer[s].resize(num_nodes);
        this->round_robin_turn[s].resize(num_nodes);
        this->ejected_flit_queue[s].resize(num_nodes);

        for (int n=0; n<num_nodes; ++n)
        {
            this->boundary_buffer[s][n].resize(vcs);
            this->ejection_buffer[s][n].resize(vcs);
        }
    }
    this->traffic_manager = TrafficManager::New(this, config, nets);
    this->traffic_manager->Init();
}

Stats* Interface::GetStats(const std::string& name)
{
    Stats* test = this->traffic_manager->getStats(name);

    if(test==0)
        std::cout
        << "warning statistics " << name
        << " not found" << std::endl;
    return test;
}

void Interface::run()
{
    ++(this->clk);
    this->traffic_manager->_Step();
}

bool Interface::is_full(uint32_t nid, uint32_t subnet, uint32_t size) const
{
    size += HEADER_SIZE;
    uint32_t expected_size = traffic_manager->_input_queue[subnet][nid][0].size() +
                            (size / flit_size + ((size%flit_size) > 0));
    return expected_size > input_buffer_capacity;
}

void Interface::push(void *packet, uint32_t subnet, 
                     uint64_t addr, int bytes, Type type, int src, int dst)
{
    traffic_manager->_GeneratePacket(type, packet, addr, bytes, HEADER_SIZE, subnet, 
                                     0, traffic_manager->_time, src, dst);
}

void Interface::Transfer2BoundaryBuffer(uint32_t subnet, uint32_t output)
{
    Flit* flit;

    for (int vc=0; vc<vcs; ++vc)
    {
        if (!ejection_buffer[subnet][output][vc].empty() && 
           (boundary_buffer[subnet][output][vc].size() < boundary_buffer_capacity))
        {
          flit = ejection_buffer[subnet][output][vc].front();
          assert(flit);

          this->ejection_buffer[subnet][output][vc].pop();
          this->boundary_buffer[subnet][output][vc].push(flit->data, flit->tail);
          this->ejected_flit_queue[subnet][output].push(flit);

          if (flit->head)
            assert(flit->dest == output);
        }
    }
}

void Interface::WriteOutBuffer(uint32_t subnet, int output, Flit *flit)
{
    int vc = flit->vc;
    assert(ejection_buffer[subnet][output][vc].size() < ejection_buffer_capacity);
    this->ejection_buffer[subnet][output][vc].push(flit);
}

Flit *Interface::GetEjectedFlit(uint32_t subnet, uint32_t nid)
{
    Flit *flit = nullptr;
    if (!ejected_flit_queue[subnet][nid].empty())
    {
        flit = ejected_flit_queue[subnet][nid].front();
        this->ejected_flit_queue[subnet][nid].pop();
    }
    return flit;
}

bool Interface::is_empty(uint32_t nid, uint32_t subnet) const
{
  return std::find_if(boundary_buffer[subnet][nid].begin(),
                      boundary_buffer[subnet][nid].end(), [](const BoundaryBufferItem& item) {
                          return !item.is_empty();
                      }) == boundary_buffer[subnet][nid].end();
}

const void *Interface::top(uint32_t nid, uint32_t subnet) const
{
    int turn = round_robin_turn[subnet][nid];

    for (int vc=0; vc<vcs; ++vc)
    {
        if (!boundary_buffer[subnet][nid][turn].is_empty())
          return boundary_buffer[subnet][nid][turn].top();
        turn = (turn+1) % vcs;
    }
    return nullptr;
}

// Pop from compute node clock domain
void Interface::pop(uint32_t nid, uint32_t subnet)
{
    int turn = round_robin_turn[subnet][nid];
    void *data = nullptr;
  
    int vc = 0;
    for (vc = 0; vc < vcs; ++vc)
    {
        if (!boundary_buffer[subnet][nid][turn].is_empty())
        {
            this->boundary_buffer[subnet][nid][turn].pop();
            break;
        } else
            turn = (turn+1) % vcs;
    }
    if (vc==vcs)
        this->round_robin_turn[subnet][nid] = turn;
}

void Interface::print_stats()
{
    this->traffic_manager->UpdateStats();
    this->traffic_manager->DisplayStats();
}

void Interface::BoundaryBufferItem::pop()
{
    assert(!is_empty());
    auto it = std::find_if(buffer.begin(), buffer.end(),
                          [](const Buffer& buf) { return buf.is_tail; });
    assert(it != buffer.end());
    assert(it->packet != nullptr);

    this->buffer.erase(buffer.begin(), it+1);
    --(this->num_packets);
}

const void* Interface::BoundaryBufferItem::top() const
{
    assert(!is_empty());
    auto it = std::find_if(buffer.begin(), buffer.end(),
                          [](const Buffer& buf) { return buf.is_tail; });
    assert(it != buffer.end());
    assert(it->packet != nullptr);
    return it->packet;
}

void Interface::BoundaryBufferItem::push(void *packet, bool is_tail)
{
    buffer.push_back({packet, is_tail});
    this->num_packets += static_cast<uint32_t>(is_tail);
}

void Interface::initParameters()
{
    this->gPrintActivity = false;
    this->gK = 0;
    this->gN = 0;
    this->gC = 0;
    this->gNodes = 0;
    this->gTrace = false;

    this->gNumVCs = 0;
    this->gReadReqBeginVC = 0;
    this->gReadReqEndVC = 0;
    this->gWriteReqBeginVC = 0;
    this->gWriteReqEndVC = 0;
    this->gReadReplyBeginVC = 0;
    this->gReadReplyEndVC = 0;
    this->gWriteReplyBeginVC = 0;
    this->gWriteReplyEndVC = 0;
}
}