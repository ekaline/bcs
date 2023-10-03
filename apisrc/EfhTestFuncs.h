#ifndef _EFH_TEST_FUNCS_H_
#define _EFH_TEST_FUNCS_H_

#include "EfhTestTypes.h"

void  INThandler(int sig);
std::string ts_ns2str(uint64_t ts);
std::string eka_get_date ();
std::string eka_get_time ();
int createThread(const char* name, EkaServiceType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle);
int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, size_t userLength, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease);
int credRelease(EkaCredentialLease *lease, void* context);
void* onOrder(const EfhOrderMsg* msg, EfhSecUserData secData, EfhRunUserData userData);
void* onTrade(const EfhTradeMsg* msg, EfhSecUserData secData, EfhRunUserData userData);
void* onEfhGroupStateChange(const EfhGroupStateChangedMsg* msg, EfhSecUserData secData, EfhRunUserData userData);
void onException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData);
//void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fire_report_buf, size_t size);
void onFireReport (const EfcFireReport* fire_report_buf, size_t size);
void* onMd(const EfhMdHeader* msg, EfhRunUserData efhRunUserData);
void* onQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData);
void eka_create_avt_definition (char* dst, const EfhOptionDefinitionMsg* msg);
void* onOptionDefinition(const EfhOptionDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData);
void* onComplexDefinition(const EfhComplexDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData);
void* onAuctionUpdate(const EfhAuctionUpdateMsg* msg, EfhSecUserData secData, EfhRunUserData userData);

EkaSource feedname2source(std::string feedName);
static EkaProp* feedname2prop (std::string feedName);
static size_t feedname2numProps (std::string feedName);
static size_t feedname2numGroups(std::string feedName);
int createCtxts(std::vector<TestRunGroup>&testRunGroups,std::vector<EfhInitCtx>&efhInitCtx,std::vector<EfhRunCtx>&efhRunCtx);

void print_usage(char* cmd);
#endif
