#ifndef _SFMPOSTING_H                       // duplication check
#define _SFMPOSTING_H

#include <cassert>
#include <deque>
#include <algorithm>
#include <map>
#include <tr1/unordered_map>
#include <google/sparsetable>
#include <google/sparse_hash_map>
#include <kcthread.h>
#include <common.h>
#include <templates.h>
#include <postline.h>

using google::sparsetable;
using namespace std;

namespace superfastmatch
{

  // Forward Declarations
  class Document;
  class Registry;
  class Command;
  class PostingSlot;
  class Association;
  
  struct DocPair{
    uint32_t doc_type;
    uint32_t doc_id;
    DocPair(uint32_t doc_type,uint32_t doc_id):
    doc_type(doc_type),
    doc_id(doc_id)
    {}
  };
    
  typedef struct{
    size_t operator() (const DocPair& k) const { 
      return (static_cast<uint64_t>(k.doc_type)<<32)|k.doc_id;
    }
  } DocPairHash;

  typedef struct
  {
    bool operator() (const DocPair& x, const DocPair &y) const { return (x.doc_type==y.doc_type) && (x.doc_id==y.doc_id); }
  } DocPairEq;
  
  struct DocTally{
    uint64_t count;
    uint64_t total;
    uint64_t last_seen;
    DocTally():
    count(0),
    total(0),
    last_seen(0)
    {}
  };
  
  typedef struct
  {
    bool operator()(const DocTally &lhs, const DocTally &rhs) const { return lhs.count > rhs.count;}
  } DocTallyEq;
    
  typedef sparsetable<unsigned char*,48> index_t;
  typedef unordered_map<uint32_t,uint64_t> stats_t;
  typedef unordered_map<uint32_t,stats_t> histogram_t;
  typedef unordered_map<DocPair,DocTally,DocPairHash,DocPairEq> search_t;
  typedef multimap<DocTally,DocPair,DocTallyEq> inverted_search_t;

  class TaskPayload{
  public:
    enum TaskOperation{
      AddDocument,
      DeleteDocument
    };
  private:
    DocumentPtr document_;
    TaskOperation operation_;
    kc::AtomicInt64 slots_left_;
  public:
    TaskPayload(DocumentPtr document,TaskOperation operation,uint32_t slots);
    ~TaskPayload();
    
    uint64_t markSlotFinished();
    TaskOperation getTaskOperation();
    DocumentPtr getDocument();
  };

  class PostingTaskQueue : public kc::TaskQueue{
  public:
    explicit PostingTaskQueue();
  private:
    void do_task(Task* task);
  };

  class PostingTask : public kc::TaskQueue::Task{
  private:
    PostingSlot* slot_;
    TaskPayload* payload_;
  public:
    explicit PostingTask(PostingSlot* slot,TaskPayload* payload);
    ~PostingTask();
    
    PostingSlot* getSlot();
    TaskPayload* getPayload();
  };

  class PostingSlot{
  private:
    Registry* registry_;
    const uint32_t slot_number_;
    const hash_t offset_;
    const hash_t span_;
    PostLineCodec* codec_;
    PostLine line_;
    index_t index_;
    kc::RWLock index_lock_;
    PostingTaskQueue queue_;
    
  public:
    PostingSlot(Registry* registry,uint32_t slot_number);
    ~PostingSlot();
    
    bool alterIndex(DocumentPtr doc,TaskPayload::TaskOperation operation);
    bool searchIndex(DocumentPtr doc,search_t& results);

    uint64_t addTask(TaskPayload* payload);
    uint32_t getTaskCount();
    size_t getHashCount();

    uint32_t fill_list_dictionary(TemplateDictionary* dict,hash_t start);
    void fillHistograms(histogram_t& hash_hist,histogram_t& gaps_hist);
  };

  class Posting{
  private:    
    Registry* registry_;
    vector<PostingSlot*> slots_;
    uint32_t doc_count_;
    bool ready_;
        
  public:
    Posting(Registry* registry);
    ~Posting();
    
    bool init();
    size_t getHashCount();
    void wait(size_t queue_length);
    void searchIndex(DocumentPtr doc,search_t& results,inverted_search_t& pruned_results);
    // Following three methods return the current queue length for all slots combined   
    uint64_t alterIndex(DocumentPtr doc,TaskPayload::TaskOperation operation);
    uint64_t addDocument(DocumentPtr doc);
    uint64_t deleteDocument(DocumentPtr doc);
    uint64_t associateDocument(DocumentPtr doc);
    bool isReady();

    void fill_search_dictionary(DocumentPtr doc,TemplateDictionary* dict);
    void fill_status_dictionary(TemplateDictionary* dict);
    void fill_list_dictionary(TemplateDictionary* dict,hash_t start);
    void fill_histogram_dictionary(TemplateDictionary* dict);
  };
}

#endif
