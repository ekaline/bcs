#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <linux/sockios.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>
#include <time.h>

#include "EkaFh.h"
#include "EkaFhBook.h"
#include "EkaFhRunGroup.h"
#include "EkaStrUtilities.h"
#include "EkaUdpChannel.h"

#include "EkaFhBatsGr.h"
#include "EkaFhBoxGr.h"
#include "EkaFhPlrGr.h"

#include "EkaCore.h"
#include "EkaHwCaps.h"
#include "eka_fh_q.h"

/* ############################################# */

int EkaFh::setId(EfhCtx *pEfhCtx, EkaSource ec,
                 uint8_t numFh) {
  assert(pEfhCtx != NULL);
  dev = pEfhCtx->dev;
  assert(dev != NULL);

  exch = ec;
  feed_ver = EFH_EXCH2FEED(exch);
  //  full_book = EFH_EXCH2FULL_BOOK(exch);
  id = numFh;
  if (exch == EkaSource::kInvalid)
    on_error("exch (%d) == EkaSource::kInvalid", (int)exch);

  return 0;
}
/* ############################################# */

uint8_t EkaFh::getGrId(const uint8_t *pkt) {
  EkaIpHdr *ipHdr = (EkaIpHdr *)(pkt - 8 - 20);
  uint32_t ip = ipHdr->dest;
  for (uint8_t i = 0; i < groups; i++)
    if (b_gr[i]->mcast_ip == ip &&
        b_gr[i]->mcast_port == EKA_UDPHDR_DST((pkt - 8)))
      return i;

  EKA_WARN("ip 0x%08x %s not found", ip, EKA_IP2STR(ip));

  return 0xFF;
}
/* ############################################# */
EkaOpResult EkaFh::init(const EfhInitCtx *pEfhInitCtx,
                        uint8_t numFh) {
  c = pEfhInitCtx->coreId;
  noTob = pEfhInitCtx->noTob;
  getTradeTimeCb = pEfhInitCtx->getTradeTime;
  getTradeTimeCtx = pEfhInitCtx->getTradeTimeCtx;

  if ((dev->ekaHwCaps->hwCaps.core.bitmap_md_cores &
       (1 << c)) == 0) {
    on_error(
        "Core %u is not enabled in FPGA for Market Data RX",
        c);
  }

  assert(pEfhInitCtx->numOfGroups <= EKA_FH_GROUPS);
  groups = pEfhInitCtx->numOfGroups;

  print_parsed_messages = pEfhInitCtx->printParsedMessages;

  for (uint i = 0; i < pEfhInitCtx->ekaProps->numProps;
       i++) {
    EkaProp &ekaProp{pEfhInitCtx->ekaProps->props[i]};
    //    EKA_DEBUG("ekaProp.szKey = %s, ekaProp.szVal =
    //    %s",ekaProp.szKey,ekaProp.szVal); fflush(stderr);
    EkaFhAddConf r =
        conf_parse(ekaProp.szKey, ekaProp.szVal);
    if (ekaFhAddConfIsErr(r)) {
      EKA_ERROR(
          "Init failed to configure due to %s at %s -- %s",
          ekaFhAddConfToString(r), ekaProp.szKey,
          ekaProp.szVal);
      return EKA_OPRESULT__ERR_INVALID_CONFIG;
    } else if (r != EkaFhAddConf::CONF_SUCCESS) {
      EKA_WARN("Config %s at %s -- %s",
               ekaFhAddConfToString(r), ekaProp.szKey,
               ekaProp.szVal);
    }
  }
  for (uint i = 0; i < groups; i++) {
    if (auth_set && !b_gr[i]->auth_set) {
      memcpy(b_gr[i]->auth_user, auth_user,
             sizeof(auth_user));
      memcpy(b_gr[i]->auth_passwd, auth_passwd,
             sizeof(auth_passwd));
      b_gr[i]->auth_set = true;
    }
    b_gr[i]->printConfig();
    /* EKA_DEBUG("initializing FH coreId=%d %s:%u: MCAST:
     * %s:%u, SNAPSHOT: %s:%u, RECOVERY: %s:%u, AUTH: %s:%s,
     * connectRetryDelayTime=%d", */
    /* 	      c, */
    /* 	      EKA_EXCH_DECODE(b_gr[i]->exch), */
    /* 	      b_gr[i]->id, */
    /* 	      EKA_IP2STR(b_gr[i]->mcast_ip),
     * b_gr[i]->mcast_port, */
    /* 	      EKA_IP2STR(b_gr[i]->snapshot_ip),be16toh(b_gr[i]->snapshot_port),
     */
    /* 	      EKA_IP2STR(b_gr[i]->recovery_ip),be16toh(b_gr[i]->recovery_port),
     */
    /* 	      b_gr[i]->auth_set ?
     * std::string(b_gr[i]->auth_user,sizeof(b_gr[i]->auth_user)).c_str()
     * : "NOT SET", */
    /* 	      b_gr[i]->auth_set ?
     * std::string(b_gr[i]->auth_passwd,sizeof(b_gr[i]->auth_passwd)).c_str()
     * : "NOT SET", */
    /* 	      b_gr[i]->connectRetryDelayTime */
    /* 	      ); */
  }
  any_group_getting_snapshot = false;

  EKA_DEBUG("%s is Initialized: coreId = %u, feed_ver = "
            "%s, %u MC groups",
            EKA_EXCH_DECODE(exch), c,
            EKA_FEED_VER_DECODE(feed_ver), groups);

  return EKA_OPRESULT__OK;
}

/* ############################################# */

/* void EkaFh::send_igmp(bool join_leave, volatile bool
 * igmp_thread_active) { */
/*   //  EKA_DEBUG("Sending IGMP %s", join_leave ? "JOIN" :
 * "LEAVE"); */
/*   for (auto i = 0; i < groups; i++) { */
/*     if (b_gr[i] == NULL) continue; */
/*     if (join_leave && ! igmp_thread_active) break; //
 * dont send JOIN */
/*     b_gr[i]->send_igmp(join_leave); */
/*     usleep (10); */
/*   } */
/*   return; */
/* } */
/* ############################################# */
int EkaFh::stop() {
  EKA_DEBUG("Stopping %s (fhId=%u)", EKA_EXCH_DECODE(exch),
            id);
  for (uint i = 0; i < groups; i++) {
    if (b_gr[i] != NULL)
      b_gr[i]->stop();
  }
  return 0;
}

/* ############################################# */
EkaFh::~EkaFh() {
  EKA_DEBUG("destroying %s:%u", EKA_EXCH_DECODE(exch), id);
  for (uint i = 0; i < groups; i++) {
    if (b_gr[i] == NULL)
      continue;
    delete b_gr[i];
    b_gr[i] = NULL;
  }
}

/* ############################################# */

int EkaFh::openGroups(EfhCtx *pEfhCtx,
                      const EfhInitCtx *pEfhInitCtx) {
  assert(pEfhInitCtx->numOfGroups <= EKA_FH_GROUPS);
  groups = pEfhInitCtx->numOfGroups;

  for (uint8_t i = 0; i < groups; i++) {
    b_gr[i] = addGroup();
    if (b_gr[i] == NULL)
      on_error("b_gr[i] == NULL");

    b_gr[i]->init(pEfhCtx, pEfhInitCtx, this, i, exch);
    b_gr[i]->bookInit();

    EKA_LOG("%s:%u initialized as %s:%u at runGroups",
            EKA_EXCH_DECODE(exch), b_gr[i]->id,
            EKA_EXCH_DECODE(b_gr[i]->exch), i);
  }
  return 0;
}

/* ############################################# */

static inline uint8_t nextQ(uint8_t c, uint first,
                            uint max) {
  if (max == 1 || c == max - 1)
    return first;
  return c + 1;
}

/* ############################################# */

EkaFhGroup *EkaFh::nextGrToProcess(uint first,
                                   uint numGroups) {
  uint8_t curr = nextQ(lastServed, first, numGroups);
  for (uint8_t i = 0; i < numGroups; i++) {
    if (b_gr[curr] == NULL)
      on_error("%s: numGroups=%u, but b_gr[%u] == NULL",
               EKA_EXCH_DECODE(exch), numGroups, curr);
    if (b_gr[curr]->q == NULL)
      on_error("%s: b_gr[%u]->q == NULL",
               EKA_EXCH_DECODE(exch), curr);

    if (b_gr[curr]->q->is_empty() ||
        b_gr[curr]->state == EkaFhGroup::GrpState::GAP) {
      curr = nextQ(curr, first, (uint)numGroups);
      continue;
    }
    lastServed = curr;
    return b_gr[curr];
  }

  return NULL;
}

/* ############################################# */

EkaOpResult EkaFh::subscribeStaticSecurity(
    uint8_t groupNum, uint64_t securityId,
    EfhSecurityType efhSecurityType,
    EfhSecUserData efhSecUserData, uint64_t opaqueAttrA,
    uint64_t opaqueAttrB) {
  if (groupNum >= groups)
    on_error("groupNum (%u) >= groups (%u)", groupNum,
             groups);
  if (b_gr[groupNum] == NULL)
    on_error("b_gr[%u] == NULL", groupNum);

  b_gr[groupNum]->subscribeStaticSecurity(
      securityId, efhSecurityType, efhSecUserData,
      opaqueAttrA, opaqueAttrB);

  b_gr[groupNum]->numSecurities++;
  return EKA_OPRESULT__OK;
}

int EkaFh::getTradeTime(
    const EfhDateComponents *dateComponents,
    uint32_t *iso8601Date, time_t *time) {
  if (!this->getTradeTimeCb) {
    // We don't have the getTradeTimeFn function available.
    // In one corner case, when the day is explicitly
    // specified, we can still compute iso8601Date.
    if (dateComponents->dayMethod ==
        EfhDayMethod::kDirectDay) {
      *iso8601Date = dateComponents->year * 10'000 +
                     dateComponents->month * 100 +
                     dateComponents->day;
    } else
      *iso8601Date = 0;
    // FIXME: we can approximate the time with localtime_r
    // and some basic algorithms. This would work except we
    // don't know the holiday schedule. For now, just rely
    // on getTradeTime support.
    *time = 0;
    errno = ENOSYS;
    return -1;
  }

  return getTradeTimeCb(dateComponents, iso8601Date, time,
                        getTradeTimeCtx);
}

EkaOpResult EkaFh::setTradeTimeCtx(void *ctx) {
  getTradeTimeCtx = ctx;
  return EKA_OPRESULT__OK;
}

/* ############################################# */

template <std::size_t N>
inline bool matchConfKey(
    const char *(&keyParts)[N],
    std::initializer_list<const char *> keyChecks) {
  const std::size_t numChecks = keyChecks.size();
  if (numChecks == 0)
    return true;
  if (numChecks >= N)
    return false;
  if (!keyParts[numChecks - 1] || keyParts[numChecks])
    return false;

  std::size_t i = 0;
  for (const char *keyCheck : keyChecks) {
    const char *keyPart = keyParts[i++];
    if (!keyCheck)
      continue;
    if (std::strcmp(keyCheck, keyPart) != 0)
      return false;
  }

  return true;
}

inline bool parseIpAddress(const char *value, uint32_t *ip,
                           uint16_t *port,
                           const bool portToBigEndian,
                           bool *set) {
  char buf[25];
  if (!tryCopyCString(buf, value))
    return false;
  char *portStart = std::strchr(buf, ':');
  if (!portStart)
    return false;
  *(portStart++) = '\0';

  if (!inet_aton(buf, (struct in_addr *)ip))
    return false;
  if (!parseFullStrToNum(portStart, port))
    return false;
  if (portToBigEndian)
    *port = htobe16(*port);
  if (set)
    *set = true;
  return true;
}

inline bool parseTimeMinutes(const char *in, int *tHour,
                             int *tMinute) {
  auto [p1, ec1] =
      std::from_chars(in, in + strlen(in), *tHour, 10);
  auto [p2, ec2] = std::from_chars(p1 + 1, in + strlen(in),
                                   *tMinute, 10);

  if (*p1 != ':' || p2 || (int)ec1 || (int)ec2)
    return false;

  return true;
}

template <std::size_t U, std::size_t P>
inline bool
parseUserPassword(const char *value, char (&user)[U],
                  char (&pass)[P], const char padding) {
  const char *divider = std::strchr(value, ':');
  if (!divider)
    return false;

  const char *userStart = value;
  const std::size_t userLen = divider - value;
  if (userLen > U)
    return false;

  const char *passStart = divider + 1;
  const std::size_t passLen = std::strlen(passStart);
  if (passLen > P)
    return false;

  // Check for second colon
  if (std::memchr(passStart, ':', passLen))
    return false;

  std::memcpy(user, userStart, userLen);
  std::memset(user + userLen, padding, U - userLen);
  std::memcpy(pass, passStart, passLen);
  std::memset(pass + passLen, padding, P - passLen);
  return true;
}

inline bool parseBool(const char *value, bool *target) {
  if (std::strcmp(value, "0") == 0 ||
      std::strcmp(value, "false") == 0) {
    *target = false;
    return true;
  }
  if (std::strcmp(value, "1") == 0 ||
      std::strcmp(value, "true") == 0) {
    *target = true;
    return true;
  }
  return false;
}

inline EkaFhGroup *getGroup(EkaFh &fh, const char *grId,
                            const char *key,
                            const char *value) {
  uint8_t gr;
  if (!parseFullStrToNum(grId, &gr)) {
    on_warning(
        "%s -- %s : ignoring invalid number for group_id",
        key, value);
    return NULL;
  }
  if (gr >= fh.groups) {
    on_warning(
        "%s -- %s : ignoring group_id %d >= groups (=%u)",
        key, value, gr, fh.groups);
    return NULL;
  }
  if (!fh.b_gr[gr])
    on_error("b_gr[%u] == NULL", gr);
  return fh.b_gr[gr];
};

template <typename Group>
inline Group *getGroupDowncast(EkaFh &fh, const char *grId,
                               const char *key,
                               const char *value) {
  EkaFhGroup *group = getGroup(fh, grId, key, value);
  if (!group)
    return NULL;
  Group *downcast = dynamic_cast<Group *>(group);
  if (!downcast)
    on_error("b_gr[%u] is not %s", group->id,
             typeid(Group).name());
  return downcast;
}

EkaFhAddConf EkaFh::conf_parse(const char *key,
                               const char *value) {
  // parsing KEY
  char key_buf[200];
  if (!tryCopyCString(key_buf, key)) {
    return EkaFhAddConf::UNKNOWN_KEY;
  }
  constexpr std::size_t MaxKeyParts = 10;
  const char *k[MaxKeyParts] = {};
  {
    std::size_t i = 0;
    k[i] = strtok(key_buf, ".");
    while (k[i++] != NULL) {
      if (i >= MaxKeyParts)
        return EkaFhAddConf::UNKNOWN_KEY;
      k[i] = strtok(NULL, ".");
    }
  }

  const EkaSourceType sourceType = getEkaSourceType(exch);
  const char *exchName = EKA_EXCH_DECODE(exch);

#define TRY_PARSE_NUM(STR, OUT, ERR)                       \
  if (!parseFullStrToNum(STR, &OUT)) {                     \
    on_warning("%s -- %s : invalid number for " #OUT, key, \
               value);                                     \
    return ERR;                                            \
  }

  //---------------------------------------------------------
  // efh.pin_packet_buffer
  // k[0] k[1]   k[2] k[3] k[4]   k[5]
  if (matchConfKey(k, {"efh", "pin_packet_buffer"})) {
    this->pinPacketBuffer = strcmp(value, "true") == 0;
    return EkaFhAddConf::CONF_SUCCESS;
  }

  //---------------------------------------------------------
  // efh.NOM_ITTO.group.X.snapshot.connectRetryDelayTime,
  // <numSec> k[0] k[1]   k[2] k[3] k[4]   k[5]
  if (matchConfKey(k,
                   {"efh", exchName, "group", NULL,
                    "snapshot", "connectRetryDelayTime"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    TRY_PARSE_NUM(value, group->connectRetryDelayTime,
                  EkaFhAddConf::WRONG_VALUE);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.NOM_ITTO.group.X.snapshot.auth, user:passwd
  // k[0] k[1]   k[2] k[3] k[4]   k[5]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "snapshot", "auth"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    char string_filler =
        EFH_EXCH2FEED(exch) == EfhFeedVer::kXDP ? '\0'
                                                : ' ';
    if (!parseUserPassword(value, group->auth_user,
                           group->auth_passwd,
                           string_filler)) {
      on_warning("%s -- %s : user:pass pair has wrong size",
                 key, value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    //      EKA_DEBUG ("auth credentials for %s:%u are set
    //      to |%s|:|%s|",k[1],gr,b_gr[gr]->auth_user +
    //      '\0',b_gr[gr]->auth_passwd + '\0');
    group->auth_set = true;

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.C1_PITCH.group.X.recovery.grpAuth, user:passwd
  // k[0] k[1]   k[2] k[3] k[4]   k[5]
  if (sourceType == EkaSourceType::kCBOE_PITCH &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "recovery", "grpAuth"})) {
    auto *group = getGroupDowncast<EkaFhBatsGr>(*this, k[3],
                                                key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseUserPassword(value, group->grpUser,
                           group->grpPasswd, ' ')) {
      on_warning("%s -- %s : user:pass pair has wrong size",
                 key, value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    /* EKA_DEBUG ("%s:%u: %s set to |%s|:|%s|", */
    /* 		 k[1],gr,k[5],group->grpUser +
     * '\0',group->grpPasswd + '\0'); */
    group->grpSet = true;

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.NOM_ITTO.snapshot.auth, user:passwd
  // k[0] k[1]   k[2]     k[3]
  if (matchConfKey(k,
                   {"efh", exchName, "snapshot", "auth"})) {
    char string_filler =
        EFH_EXCH2FEED(exch) == EfhFeedVer::kXDP ? '\0'
                                                : ' ';

    if (!parseUserPassword(value, auth_user, auth_passwd,
                           string_filler)) {
      on_warning("%s -- %s : user:pass pair has wrong size",
                 key, value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    //      EKA_DEBUG ("auth credentials for %s are set to
    //      %s:%s",k[1],v[0] +'\0',v[1]+'\0');
    auth_set = true;

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  //---------------------------------------------------------
  // efh.BOX_HSVF.group.X.gapsLimit, "1"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "gapsLimit"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    TRY_PARSE_NUM(value, group->gapsLimit,
                  EkaFhAddConf::WRONG_VALUE);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.C1_PITCH.group.X.unit, "1"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if (sourceType == EkaSourceType::kCBOE_PITCH &&
      matchConfKey(
          k, {"efh", exchName, "group", NULL, "unit"})) {
    auto *group = getGroupDowncast<EkaFhBatsGr>(*this, k[3],
                                                key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    TRY_PARSE_NUM(value, group->batsUnit,
                  EkaFhAddConf::WRONG_VALUE);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.BATS_PITCH.queue_capacity, "1048576"
  // k[0] k[1]       k[2]
  // efh.BATS_PITCH.group.X.queue_capacity, "1048576"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if (matchConfKey(k,
                   {"efh", exchName, "queue_capacity"}) ||
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "queue_capacity"})) {

    TRY_PARSE_NUM(value, qsize, EkaFhAddConf::WRONG_VALUE);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.BATS_PITCH.group.X.snapshot.sessionSubID, "0285"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if (sourceType == EkaSourceType::kCBOE_PITCH &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "snapshot", "sessionSubID"})) {
    auto *group = getGroupDowncast<EkaFhBatsGr>(*this, k[3],
                                                key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    constexpr std::size_t len = sizeof(group->sessionSubID);
    if (std::strlen(value) != len) {
      on_warning("%s -- %s : sessionSubID is not right "
                 "length (%d)",
                 key, value, (int)len);
      return EkaFhAddConf::WRONG_VALUE;
    }
    std::memcpy(group->sessionSubID, value, len);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.C1_PITCH.group.X.recovery.grpSessionSubID, "0285"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if (sourceType == EkaSourceType::kCBOE_PITCH &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "recovery", "grpSessionSubID"})) {
    auto *group = getGroupDowncast<EkaFhBatsGr>(*this, k[3],
                                                key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    constexpr std::size_t len =
        sizeof(group->grpSessionSubID);
    if (std::strlen(value) != len) {
      on_warning("%s -- %s : grpSessionSubID is not right "
                 "length (%d)",
                 key, value, (int)len);
      return EkaFhAddConf::WRONG_VALUE;
    }
    std::memcpy(group->grpSessionSubID, value, len);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.NOM_ITTO.group.X.mcast.addr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "mcast", "addr"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->mcast_ip,
                        &group->mcast_port, false,
                        &group->mcast_set)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }
    //      EKA_DEBUG ("%s %s for %s:%u is set to
    //      %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.NOM_ITTO.group.X.snapshot.addr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "snapshot", "addr"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->snapshot_ip,
                        &group->snapshot_port, true,
                        &group->snapshot_set)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }
    //      EKA_DEBUG ("%s %s for %s:%u is set to
    //      %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.C1_PITCH.group.X.recovery.grpAddr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (sourceType == EkaSourceType::kCBOE_PITCH &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "recovery", "grpAddr"})) {
    auto *group = getGroupDowncast<EkaFhBatsGr>(*this, k[3],
                                                key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->grpIp,
                        &group->grpPort, true, nullptr)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }
    /* EKA_DEBUG ("%s:%u: %s is set to %s:%u", */
    /* 		 k[1],gr,k[5],EKA_IP2STR(batsGr->grpIp),be16toh(batsGr->grpPort));
     */

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.NOM_ITTO.group.X.recovery.addr, x.x.x.x:xxxx
  // k[0] k[1]    k[2] k[3] k[4]   k[5]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "recovery", "addr"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->recovery_ip,
                        &group->recovery_port, true,
                        &group->recovery_set)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }
    //      EKA_DEBUG ("%s %s for %s:%u is set to
    //      %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.<exch>.group.X.products,
  // <comma-separated-product-keys> k[0] k[1]   k[2]  k[3]
  // k[4]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "products"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    constexpr int MaxProductsLen = 200;
    char val_buf[MaxProductsLen + 1];
    if (!tryCopyCString(val_buf, value)) {
      on_warning("%s -- %s : products list was too large "
                 "to parse (max str len = %d)",
                 key, value, MaxProductsLen);
      return EkaFhAddConf::WRONG_VALUE;
    }

    for (const char *n = strtok(val_buf, ","); n;
         n = strtok(NULL, ",")) {
      while (*n == ' ')
        n++; // skipping leading spaces
      const int mask = lookupProductMask(n);
      if (mask == NoSuchProduct) {
        on_warning(
            "%s -- %s : product token `%s` not recognized",
            key, value, n);
        return EkaFhAddConf::WRONG_VALUE;
      }
      group->productMask |= mask;
    }
    EKA_DEBUG("%s:%u: ProductMask is set to %x", exchName,
              group->id, group->productMask);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.<exch>.group.X.useDefinitionsFile, "0" or "1"
  // k[0] k[1]  k[2] k[3] k[4]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "useDefinitionsFile"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseBool(value, &group->useDefinitionsFile)) {
      on_warning("%s -- %s : invalid boolean, should be "
                 "1/0 or true/false",
                 key, value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    EKA_DEBUG("%s:%u: useDefinitionsFile is set to %d",
              exchName, group->id,
              group->useDefinitionsFile);
    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.BOX_HSVF.group.X.mcast.line   ,"11"
  // k[0] k[1]    k[2] k[3] k[4]   k[5]
  if (sourceType == EkaSourceType::kBOX_HSVF &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "mcast", "line"})) {
    auto *group = getGroupDowncast<EkaFhBoxGr>(*this, k[3],
                                               key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!tryCopyPaddedString(group->line, value, ' ')) {
      on_warning(
          "%s -- %s : line ID is too long (max %d chars)",
          key, value, (int)sizeof(group->line));
      return EkaFhAddConf::WRONG_VALUE;
    }
    //      EKA_DEBUG ("%s:%u line = %s",
    //      EKA_EXCH_SOURCE_DECODE(exch),gr,v[0]);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.ARCA_PLR.group.X.refresh.tcpAddr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (sourceType == EkaSourceType::kNYSE_PLR &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "refresh", "tcpAddr"})) {
    auto *group = getGroupDowncast<EkaFhPlrGr>(*this, k[3],
                                               key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->refreshTcpIp,
                        &group->refreshTcpPort, true,
                        NULL)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    EKA_DEBUG("%s:%u: %s is set to %s:%u", exchName,
              group->id, k[5],
              EKA_IP2STR(group->refreshTcpIp),
              be16toh(group->refreshTcpPort));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.ARCA_PLR.group.X.refresh.udpAddr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (sourceType == EkaSourceType::kNYSE_PLR &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "refresh", "udpAddr"})) {
    auto *group = getGroupDowncast<EkaFhPlrGr>(*this, k[3],
                                               key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->refreshUdpIp,
                        &group->refreshUdpPort, true,
                        NULL)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    EKA_DEBUG("%s:%u: refreshUdpIp:refreshUdpPort %s is "
              "set to %s:%u",
              exchName, group->id, k[5],
              EKA_IP2STR(group->refreshUdpIp),
              be16toh(group->refreshUdpPort));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.ARCA_PLR.group.X.retrans.tcpAddr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (sourceType == EkaSourceType::kNYSE_PLR &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "retrans", "tcpAddr"})) {
    auto *group = getGroupDowncast<EkaFhPlrGr>(*this, k[3],
                                               key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->retransTcpIp,
                        &group->retransTcpPort, true,
                        NULL)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    EKA_DEBUG("%s:%u: retransTcpIp:retransTcpPort %s is "
              "set to %s:%u",
              exchName, group->id, k[5],
              EKA_IP2STR(group->retransTcpIp),
              be16toh(group->retransTcpPort));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.ARCA_PLR.group.X.retrans.udpAddr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (sourceType == EkaSourceType::kNYSE_PLR &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "retrans", "udpAddr"})) {
    auto *group = getGroupDowncast<EkaFhPlrGr>(*this, k[3],
                                               key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseIpAddress(value, &group->retransUdpIp,
                        &group->retransUdpPort, true,
                        NULL)) {
      on_warning("%s -- %s : IP address is not valid", key,
                 value);
      return EkaFhAddConf::WRONG_VALUE;
    }

    EKA_DEBUG("%s:%u: retransUdpIp:retransUdpPort %s is "
              "set to %s:%u",
              exchName, group->id, k[5],
              EKA_IP2STR(group->retransUdpIp),
              be16toh(group->retransUdpPort));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.ARCA_PLR.group.X.SourceId, xxxx
  // k[0] k[1]    k[2] k[3] k[4]    v[0]
  if (sourceType == EkaSourceType::kNYSE_PLR &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "SourceId"})) {
    auto *group = getGroupDowncast<EkaFhPlrGr>(*this, k[3],
                                               key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!tryCopyPaddedString(group->sourceId, value)) {
      on_warning("%s -- %s : value too long for source ID "
                 "(max len = %u)",
                 key, value,
                 (unsigned)sizeof(group->sourceId));
      return EkaFhAddConf::WRONG_VALUE;
    }

    EKA_DEBUG("%s:%u: %s is set to \'%s\'", exchName,
              group->id, k[4], group->sourceId);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.ARCA_PLR.group.X.ChannelId, x
  // k[0] k[1]    k[2] k[3] k[4]    v[0]
  if (sourceType == EkaSourceType::kNYSE_PLR &&
      matchConfKey(k, {"efh", exchName, "group", NULL,
                       "ChannelId"})) {
    auto *group = getGroupDowncast<EkaFhPlrGr>(*this, k[3],
                                               key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    TRY_PARSE_NUM(value, group->channelId,
                  EkaFhAddConf::WRONG_VALUE);

    EKA_DEBUG("%s:%u: ChannelId is set to %d", exchName,
              group->id, group->channelId);

    return EkaFhAddConf::CONF_SUCCESS;
  }

  //---------------------------------------------------------
  // efh.NOM_ITTO.group.X.staleDataNsThreshold,
  // <numSec> k[0] k[1]   k[2] k[3] k[4]   k[5]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "staleDataNsThreshold"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    TRY_PARSE_NUM(value, group->staleDataNsThreshold,
                  EkaFhAddConf::WRONG_VALUE);

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.<EXCH>.group.X.check.md.start, HH:MM
  // k[0] k[1]  k[2] k[3] k[4] k[5] k[6]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "check", "md", "start"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseTimeMinutes(value, &group->mdCheckStartHour,
                          &group->mdCheckStartMinute)) {
      on_warning("Invalid HH:MM format  \'%s\' -- \'%s\'",
                 key, value);
      return EkaFhAddConf::WRONG_VALUE;
    }
    //      EKA_DEBUG ("%s %s for %s:%u is set to
    //      %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));

    return EkaFhAddConf::CONF_SUCCESS;
  }
  //---------------------------------------------------------
  // efh.<EXCH>.group.X.check.md.end, HH:MM
  // k[0] k[1]  k[2] k[3] k[4] k[5] k[6]
  if (matchConfKey(k, {"efh", exchName, "group", NULL,
                       "check", "md", "end"})) {
    EkaFhGroup *group = getGroup(*this, k[3], key, value);
    if (!group)
      return EkaFhAddConf::IGNORED;

    if (!parseTimeMinutes(value, &group->mdCheckEndHour,
                          &group->mdCheckEndMinute)) {
      on_warning("Invalid HH:MM format  \'%s\' -- \'%s\'",
                 key, value);
      return EkaFhAddConf::WRONG_VALUE;
    }
    //      EKA_DEBUG ("%s %s for %s:%u is set to
    //      %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));

    return EkaFhAddConf::CONF_SUCCESS;
  }
#undef TRY_PARSE_NUM

  //---------------------------------------------------------
  return EkaFhAddConf::UNKNOWN_KEY;
}
/* ############################################# */

EkaOpResult EkaFh::initGroups(EfhCtx *pEfhCtx,
                              const EfhRunCtx *pEfhRunCtx,
                              EkaFhRunGroup *runGr) {
  for (uint8_t i = 0; i < runGr->numGr; i++) {
    if (!runGr->isMyGr(pEfhRunCtx->groups[i].localId))
      on_error("pEfhRunCtx->groups[%d].localId = %u doesnt "
               "belong to %s",
               i, pEfhRunCtx->groups[i].localId,
               runGr->list2print);
    if (pEfhRunCtx->groups[i].source != exch)
      on_error("id=%u,fhId = %u, "
               "pEfhRunCtx->groups[i].source %s != exch %s",
               id, pEfhCtx->fhId,
               EKA_EXCH_SOURCE_DECODE(
                   pEfhRunCtx->groups[i].source),
               EKA_EXCH_SOURCE_DECODE(exch));

    EkaFhGroup *gr = b_gr[pEfhRunCtx->groups[i].localId];
    if (gr == NULL)
      on_error("b_gr[%u] == NULL",
               pEfhRunCtx->groups[i].localId);

    //    b_gr[i]->bookInit();
    gr->createQ(pEfhCtx, qsize);
    gr->expected_sequence = 1;

    gr->runGr = runGr;

    runGr->igmpMcJoin(gr->mcast_ip, gr->mcast_port, 0,
                      &gr->pktCnt);
    EKA_DEBUG("%s:%u: joined %s:%u for %u securities",
              EKA_EXCH_DECODE(exch), gr->id,
              EKA_IP2STR(gr->mcast_ip), gr->mcast_port,
              gr->getNumSecurities());
  }
  return EKA_OPRESULT__OK;
}

/* ############################################# */
