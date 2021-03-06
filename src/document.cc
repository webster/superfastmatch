#include "document.h"

namespace superfastmatch
{
  
  // -----------------------
  // DocumentCursor members
  // -----------------------
  
  DocumentCursor::DocumentCursor(Registry* registry):
  registry_(registry){
    cursor_=registry->getDocumentDB()->cursor();
    cursor_->jump();
  };

  DocumentCursor::~DocumentCursor(){
    delete cursor_;
  }

  bool DocumentCursor::jumpFirst(){
    return cursor_->jump();
  }

  bool DocumentCursor::jumpLast(){
    return cursor_->jump_back();
  };

  bool DocumentCursor::jump(string& key){
    return cursor_->jump(key);
  };

  DocumentPtr DocumentCursor::getNext(const int32_t state){
    string key;
    if (cursor_->get_key(&key,true)){
      return registry_->getDocumentManager()->getDocument(key,state);
    };
    return DocumentPtr();
  };

  DocumentPtr DocumentCursor::getPrevious(){
    return DocumentPtr();
  };

  uint32_t DocumentCursor::getCount(){
    return registry_->getDocumentDB()->count();
  };

  struct MetaKeyComparator {
    bool operator() (const string& lhs, const string& rhs) const{
      if (lhs==rhs)
        return false;
      if ((lhs=="title"))
        return true;
      if ((lhs=="characters") && (rhs=="title"))
        return false;
      if (lhs=="characters")
        return true;
      return lhs<rhs;
    }
  };

  void DocumentCursor::fill_list_dictionary(TemplateDictionary* dict,uint32_t doctype,uint32_t docid){
    if (getCount()==0){
      return;
    }
    TemplateDictionary* page_dict=dict->AddIncludeDictionary("PAGING");
    page_dict->SetFilename(PAGING);
    uint32_t count=0;
    DocumentPtr doc;
    char* key=new char[8];
    size_t key_length;
    uint32_t di;
    if (doctype!=0){
      uint32_t dt=kc::hton32(doctype);
      memcpy(key,&dt,4);
      cursor_->jump(key,4);
    }
    delete[] key;
    key=cursor_->get_key(&key_length,false);
    if (key!=NULL){
      memcpy(&di,key+4,4);
      di=kc::ntoh32(di);
      page_dict->SetValueAndShowSection("PAGE",toString(di),"FIRST");
    }
    if (docid!=0){
      di=kc::hton32(docid);
      memcpy(key+4,&di,4);
      cursor_->jump(key,8);
    }
    delete[] key;
    vector<DocumentPtr> docs;
    vector<string> keys;
    set<string,MetaKeyComparator> keys_set;
    while (((doc=getNext(DocumentManager::META)))&&(count<registry_->getPageSize())){
      if ((doctype!=0) && (doctype!=doc->doctype())){
        break;
      }
      docs.push_back(doc);
      if (doc->getMetaKeys(keys)){
        for (vector<string>::iterator it=keys.begin();it!=keys.end();it++){
          keys_set.insert(*it);
        } 
      }
      count++;
    }
    for (set<string>::const_iterator it=keys_set.begin(),ite=keys_set.end();it!=ite;++it){
      TemplateDictionary* keys_dict=dict->AddSectionDictionary("KEYS");
      keys_dict->SetValue("KEY",*it);
    }
    for (vector<DocumentPtr>::iterator it=docs.begin(),ite=docs.end();it!=ite;++it){
      TemplateDictionary* doc_dict = dict->AddSectionDictionary("DOCUMENT");
      doc_dict->SetIntValue("DOC_TYPE",(*it)->doctype());
      doc_dict->SetIntValue("DOC_ID",(*it)->docid()); 
      for (set<string>::const_iterator it2=keys_set.begin(),ite2=keys_set.end();it2!=ite2;++it2){
        TemplateDictionary* values_dict=doc_dict->AddSectionDictionary("VALUES");
        values_dict->SetValue("VALUE",(*it)->getMeta(&(*it2->c_str())));
      }
    }
  
    if (doc!=NULL){
      key=cursor_->get_key(&key_length,false);
      memcpy(&di,key+4,4);
      di=kc::ntoh32(di);
      page_dict->SetValueAndShowSection("PAGE",toString(di),"NEXT");
      delete[] key;
    }
    if ((doctype==0)&&(cursor_->jump_back())){
      key=cursor_->get_key(&key_length,false);
      memcpy(&di,key+4,4);
      di=kc::ntoh32(di);
      page_dict->SetValueAndShowSection("PAGE",toString(di),"LAST");
      delete[] key;
    }
  }

  // -----------------------
  // Document members
  // -----------------------

  Document::Document(const uint32_t doctype,const uint32_t docid, const bool permanent,Registry* registry):
  doctype_(doctype),docid_(docid),permanent_(permanent),empty_meta_(new string()),registry_(registry),key_(0),text_(0),clean_text_(0),metadata_(),hashes_(0),bloom_(0)
  {
    char key[8];
    uint32_t dt=kc::hton32(doctype_);
    uint32_t di=kc::hton32(docid_);
    memcpy(key,&dt,4);
    memcpy(key+4,&di,4);
    key_ = new string(key,8);
  }
  
  Document::Document(const string& key,const bool permanent,Registry* registry):
  permanent_(permanent),empty_meta_(new string()),registry_(registry),key_(0),text_(0),clean_text_(0),metadata_(0),hashes_(0),bloom_(0)
  {
    key_= new string(key);
    memcpy(&doctype_,key.data(),4);
    memcpy(&docid_,key.data()+4,4);
    doctype_=kc::ntoh32(doctype_);
    docid_=kc::ntoh32(docid_);
  }
  
  Document::~Document(){
    // cout << "Deleting " << *this <<endl;;
    if (clean_text_!=0){
      delete clean_text_;
      clean_text_=0;
    }
    if (metadata_!=0){
      delete metadata_;
      metadata_=0;
    }
    if (hashes_!=0){
      delete hashes_;
      hashes_=0;
    }
    if (bloom_!=0){
      delete bloom_;
      bloom_=0; 
    }
    if(key_!=0){
      delete key_;
      key_=0;
    }
    if (text_!=0){
      delete text_;
      text_=0;
    }
    if (empty_meta_!=0){
      delete empty_meta_;
      empty_meta_=0;
    }
  }
  
  bool Document::initMeta(){
    if (metadata_==0){
      metadata_map* tempMap = new metadata_map();
      vector<string> meta_keys;
      string meta_value;
      registry_->getMetaDB()->match_prefix(getKey(),&meta_keys);
      for (vector<string>::const_iterator it=meta_keys.begin(),ite=meta_keys.end();it!=ite;++it){
        if (not registry_->getMetaDB()->get(*it,&meta_value)){
          return false;
        };
        (*tempMap)[(*it).substr(8)]=meta_value;
      }      
      metadata_=tempMap;
    }
    return true;
  }

  bool Document::initText(){
    if (text_==0){
      text_ = new string();
      return registry_->getDocumentDB()->get(getKey(),text_);
    }
    return true;
  }
  
  bool Document::initCleanText(){
    if (clean_text_==0){
      string* tempClean=new string(getText().size(),' ');
      replace_copy_if(getText().begin(),getText().end(),tempClean->begin(),notAlphaNumeric,' ');
      transform(tempClean->begin(), tempClean->end(), tempClean->begin(), ::tolower);
      clean_text_=tempClean;
    }
    return true;
  }

  bool Document::initHashes(){
    if (hashes_==0){
      hashes_vector* tempHashes = new hashes_vector();
      uint32_t length = getCleanText().length()-registry_->getWindowSize()+1;
      hash_t white_space = registry_->getWhiteSpaceHash(false);
      uint32_t window_size=registry_->getWindowSize();
      uint32_t white_space_threshold=registry_->getWhiteSpaceThreshold();
      tempHashes->resize(length);
      hash_t hash;
      size_t i=0;
      string::const_iterator it=getCleanText().begin(),ite=(it+length);
      size_t whitespace_count=count(it,it+window_size,' ');
      for (;it!=ite;++it){
        if (*(it+window_size)==' '){
          whitespace_count++;
        }
        if (whitespace_count>white_space_threshold){
          hash=white_space;
        }else{
          hash=hashmurmur(&(*it),window_size);
        }
        (*tempHashes)[i]=hash;
        if (*it==' '){
          whitespace_count--;
        }
        i++;
      }
      hashes_=tempHashes;
    }
    return true;
  }
  
  bool Document::initBloom(){
    if (bloom_==0){
      hashes_bloom* tempBloom = new hashes_bloom();
      for (hashes_vector::const_iterator it=getHashes().begin(),ite=getHashes().end();it!=ite;++it){
        tempBloom->set(*it&0xFFFFFF);
      }
      bloom_=tempBloom;
    }
    return true;
  }
  
  bool Document::remove(){
    bool success=registry_->getDocumentDB()->remove(*key_);
    vector<string> meta_keys;
    registry_->getMetaDB()->match_prefix(getKey(),&meta_keys);
    for (vector<string>::const_iterator it=meta_keys.begin(),ite=meta_keys.end();it!=ite;++it){
      success = success && registry_->getMetaDB()->remove(*it);
    }
    return success;
  }
  
  hashes_vector& Document::getHashes(){
    if (hashes_==0){
      throw runtime_error("Hashes not initialised");
    }
    return *hashes_;
  }

  string& Document::getCleanText(){
    if (clean_text_==0){
      throw runtime_error("Clean Text not initialised");
    }
    return *clean_text_;
  }
  
  hashes_bloom& Document::getBloom(){
    if (bloom_==0){
      throw runtime_error("Bloom not initialised");
    }
    return *bloom_;
  }

  string& Document::getMeta(const string& key){
    if (metadata_==0){
      throw runtime_error("Meta not initialised");
    }
    if (metadata_->find(key)!=metadata_->end()){
      return (*metadata_)[key];
    }
    return *empty_meta_;
  }
  
  bool Document::setMeta(const string& key, const string& value){
    if (metadata_==0){
      throw runtime_error("Meta not initialised");
    }
    (*metadata_)[key]=value;
    return permanent_?registry_->getMetaDB()->set(getKey()+key,value):true;
  }
  
  bool Document::getMetaKeys(vector<string>& keys){
    keys.clear();
    if (metadata_!=0){
      for (metadata_map::const_iterator it=metadata_->begin(),ite=metadata_->end();it!=ite;it++){
        keys.push_back(it->first);
      }
      return true;
    }
    return false;
  }
  
  bool Document::setText(const string& text){
    text_=new string(text);
    return permanent_?registry_->getDocumentDB()->cas(getKey().data(),getKey().size(),NULL,0,text_->data(),text_->size()):true;
  }
  
  string& Document::getText(){
    if (text_==0){
      throw runtime_error("Text not initialised");
    }
    return *text_;
  }
  
  string& Document::getKey(){
    return *key_;
  }
  
  const uint32_t Document::doctype(){
    return doctype_;
  }
      
  const uint32_t Document::docid(){
    return docid_;
  } 
    
  ostream& operator<< (ostream& stream, Document& document) {
    stream << "Document(" << document.doctype() << "," << document.docid() << ")";
    return stream;
  }
  
  void Document::fill_document_dictionary(TemplateDictionary* dict){
    vector<string> keys;
    if (getMetaKeys(keys)){
      for (vector<string>::iterator it=keys.begin();it!=keys.end();it++){
        TemplateDictionary* meta_dict=dict->AddSectionDictionary("META");
        meta_dict->SetValue("KEY",*it);
        meta_dict->SetValue("VALUE",getMeta(&(*it->c_str())));
      } 
    }
    dict->SetValue("TEXT",getText());
    TemplateDictionary* association_dict=dict->AddIncludeDictionary("ASSOCIATION");
    association_dict->SetFilename(ASSOCIATION);
    // This belongs in Association Cursor
    // And need a key based Association Constructor
    kc::PolyDB::Cursor* cursor=registry_->getAssociationDB()->cursor();
    cursor->jump(getKey().data(),8);
    string next;
    string other_key;
    while((cursor->get_key(&next,true))&&(getKey().compare(next.substr(0,8))==0)){
      other_key=next.substr(8,8);
      DocumentPtr other = registry_->getDocumentManager()->getDocument(other_key);
      Association association(registry_,shared_from_this(),other);
      association.fill_item_dictionary(association_dict);
    }
    delete cursor;
  }
  
  bool operator< (Document& lhs,Document& rhs){
    if (lhs.doctype() == rhs.doctype()){
      return lhs.docid() < rhs.docid();
    } 
    return lhs.doctype() < rhs.doctype();
  }

  // -----------------------
  // DocumentManager members
  // -----------------------
  
  DocumentManager::DocumentManager(Registry* registry):
  registry_(registry){}

  DocumentManager::~DocumentManager(){}
  
  DocumentPtr DocumentManager::createTemporaryDocument(const string& content,const int32_t state){
    return createDocument(0,0,content,state,false);;
  }
  
  DocumentPtr DocumentManager::createPermanentDocument(const uint32_t doctype, const uint32_t docid,const string& content,const int32_t state){
    assert((doctype>0) && (docid>0));
    DocumentPtr doc = createDocument(doctype,docid,content,state,true);
    return doc;
  }

  bool DocumentManager::removePermanentDocument(DocumentPtr doc){
    assert((doc->doctype()>0) && (doc->docid()>0));
    return doc->remove();
  }

  DocumentPtr DocumentManager::getDocument(const uint32_t doctype, const uint32_t docid,const int32_t state){
    assert((doctype>0) && (docid>0));
    DocumentPtr doc(new Document(doctype,docid,true,registry_));
    initDoc(doc,state);
    return doc;
  }
  
  DocumentPtr DocumentManager::getDocument(const string& key,const int32_t state){
    DocumentPtr doc(new Document(key,true,registry_));
    initDoc(doc,state);
    return doc;
  }
  
  bool stripMeta(string lhs,string rhs){
    return lhs.substr(0,8).compare(rhs.substr(0,8))==0;
  }
  
  vector<DocumentPtr> DocumentManager::getDocuments(const uint32_t doctype,const int32_t state){
    vector<DocumentPtr> documents;
    vector<string> keys;
    if (doctype!=0){
      char key[4];
      uint32_t dt=kc::hton32(doctype);
      memcpy(key,&dt,4);
      string match(key,4);
      registry_->getMetaDB()->match_prefix(match,&keys);
    }else{
      registry_->getMetaDB()->match_prefix("",&keys);
    }
    vector<string>::iterator end=std::unique(keys.begin(),keys.end(),stripMeta);
    keys.resize(end-keys.begin());
    for(vector<string>::iterator it=keys.begin(),ite=keys.end();it!=ite;++it){
      documents.push_back(getDocument((*it).substr(0,8),state));
    }
    return documents;
  }
  
  DocumentPtr DocumentManager::createDocument(const uint32_t doctype, const uint32_t docid,const string& content,const int32_t state,const bool permanent){
    DocumentPtr doc(new Document(doctype,docid,permanent,registry_));
    metadata_map content_map;
    kt::wwwformtomap(content,&content_map);
    if (not doc->setText(content_map["text"])){
      return DocumentPtr();
    }
    content_map.erase("text");
    initDoc(doc,state|META);
    assert(doc->setMeta("characters",toString(doc->getText().size()).c_str()));
    for(metadata_map::const_iterator it=content_map.begin(),ite=content_map.end();it!=ite;++it){
      assert(doc->setMeta(it->first.c_str(),it->second.c_str()));
    }
    return doc;
  }
  
  bool DocumentManager::associateDocument(DocumentPtr doc){
    stringstream message;
    message << "Associating: " << *doc;
    registry_->getLogger()->log(Logger::DEBUG,&message);
    search_t results;
    inverted_search_t pruned_results;
    registry_->getPostings()->searchIndex(doc,results,pruned_results);
    size_t num_results=registry_->getNumResults();
    size_t count=0;
    for(inverted_search_t::iterator it2=pruned_results.begin(),ite2=pruned_results.end();it2!=ite2 && count<num_results;++it2){
      DocumentPtr other=registry_->getDocumentManager()->getDocument(it2->second.doc_type,it2->second.doc_id);
      Association association(registry_,doc,other);
      association.save();
      count++;
    } 
    return true;
  }
  
  void DocumentManager::initDoc(const DocumentPtr doc,const int32_t state){
    if (state&META)
      assert(doc->initMeta());
    if (state&TEXT)
      assert(doc->initText());
    if (state&CLEAN_TEXT)
       assert(doc->initCleanText());
    if (state&HASHES)
      assert(doc->initHashes());
    if (state&BLOOM)
      assert(doc->initBloom());
  }

}//namespace superfastmatch
