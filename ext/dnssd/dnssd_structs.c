/*
 * Copyright (c) 2004 Chad Fowler, Charles Mills, Rich Kilmer
 * Licensed under the same terms as Ruby.
 * This software has absolutely no warranty.
 */
#include "dnssd.h"

static VALUE cDNSSDFlags;
static VALUE cDNSSDReply;

static ID dnssd_iv_flags;
static ID dnssd_iv_interface;
static ID dnssd_iv_fullname;
static ID dnssd_iv_target;
static ID dnssd_iv_port;
static ID dnssd_iv_text_record;
static ID dnssd_iv_name;
static ID dnssd_iv_type;
static ID dnssd_iv_domain;
static ID dnssd_iv_service;

/* dns sd flags, flag ID's, flag names */
#define DNSSD_MAX_FLAGS 9

static const DNSServiceFlags dnssd_flag[DNSSD_MAX_FLAGS] = {
  kDNSServiceFlagsMoreComing,

  kDNSServiceFlagsAdd,
  kDNSServiceFlagsDefault,

  kDNSServiceFlagsNoAutoRename,

  kDNSServiceFlagsShared,
  kDNSServiceFlagsUnique,

  kDNSServiceFlagsBrowseDomains,
  kDNSServiceFlagsRegistrationDomains,

  kDNSServiceFlagsLongLivedQuery
};

static const char *dnssd_flag_name[DNSSD_MAX_FLAGS] = {
  "more_coming",
  "add",
  "default",
  "no_auto_rename",
  "shared",
  "unique",
  "browse_domains",
  "registration_domains",
  "long_lived_query"
};

VALUE
dnssd_create_fullname(const char *name, const char *regtype, const char *domain, int err_flag) {
  char buffer[kDNSServiceMaxDomainName];
  if ( DNSServiceConstructFullName(buffer, name, regtype, domain) ) {
    static const char msg[] = "could not construct full service name";
    if (err_flag) {
      rb_raise(rb_eArgError, msg);
    } else {
      VALUE buf;
      rb_warn(msg);
      /* just join them all */
      buf = rb_str_buf_new2(name);
      rb_str_buf_cat2(buf, regtype);
      rb_str_buf_cat2(buf, domain);
      return buf;
    }
  }
  buffer[kDNSServiceMaxDomainName - 1] = '\000'; /* just in case */
  return rb_str_new2(buffer);
}

#if 0
  static void
quote_and_append(VALUE buf, VALUE str)
{
  const char *ptr;
  long i, last_mark, len;

  ptr = RSTRING_PTR(str);
  len = RSTRING_LEN(str);
  last_mark = 0;
  /* last character should be '.' */
  for (i=0; i<len-1; i++) {
    if (ptr[i] == '.') {
      /* write 1 extra character and replace it with '\\' */
      rb_str_buf_cat(buf, ptr + last_mark, i + 1 - last_mark);
      RSTRING_PTR(buf)[i] = '\\';
      last_mark = i;
    }
  }
  rb_str_buf_cat(buf, ptr + last_mark, len - last_mark);
}
#endif

static VALUE
dnssd_join_names(int argc, VALUE *argv) {
  int i;
  VALUE buf;
  long len = 0;

  for (i=0; i<argc; i++) {
    argv[i] = StringValue(argv[i]);
    len += RSTRING_LEN(argv[i]);
  }
  buf = rb_str_buf_new(len);
  for (i=0; i<argc; i++) {
    rb_str_buf_append(buf, argv[i]);
  }
  return buf;
}

static void
reply_add_names(VALUE self, const char *name,
    const char *regtype, const char *domain) {
  rb_ivar_set(self, dnssd_iv_name, rb_str_new2(name));
  rb_ivar_set(self, dnssd_iv_type, rb_str_new2(regtype));
  rb_ivar_set(self, dnssd_iv_domain, rb_str_new2(domain));
  rb_ivar_set(self, dnssd_iv_fullname, dnssd_create_fullname(name, regtype, domain, 0));
}

static void
reply_add_names2(VALUE self, const char *fullname) {
  VALUE fn = rb_str_new2(fullname);
  rb_funcall(self, rb_intern("set_fullname"), 1, fn);
}

static void
reply_set_interface(VALUE self, uint32_t interface) {
  VALUE if_value;
  char buffer[IF_NAMESIZE];
  if (if_indextoname(interface, buffer)) {
    if_value = rb_str_new2(buffer);
  } else {
    if_value = ULONG2NUM(interface);
  }
  rb_ivar_set(self, dnssd_iv_interface, if_value);
}

static void
reply_set_tr(VALUE self, uint16_t txt_len, const char *txt_rec) {
  rb_ivar_set(self, dnssd_iv_text_record, dnssd_tr_new((long)txt_len, txt_rec));
}

static VALUE
reply_new(VALUE service, DNSServiceFlags flags) {
  VALUE self = rb_obj_alloc(cDNSSDReply);
  rb_ivar_set(self, dnssd_iv_service, service);
  rb_ivar_set(self, dnssd_iv_flags,
              rb_funcall(cDNSSDFlags, rb_intern("new"), 1, flags));
  return self;
}

VALUE
dnssd_domain_enum_new(VALUE service, DNSServiceFlags flags,
    uint32_t interface, const char *domain) {
  VALUE d, self = reply_new(service, flags);
  reply_set_interface(self, interface);
  d = rb_str_new2(domain);
  rb_ivar_set(self, dnssd_iv_domain, d);
  rb_ivar_set(self, dnssd_iv_fullname, d);
  return self;
}

VALUE
dnssd_browse_new(VALUE service, DNSServiceFlags flags, uint32_t interface,
    const char *name, const char *regtype, const char *domain) {
  VALUE self = reply_new(service, flags);
  reply_set_interface(self, interface);
  reply_add_names(self, name, regtype, domain);
  return self;
}

#if 0
  static VALUE
dnssd_gethostname(void)
{
#if HAVE_GETHOSTNAME
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif
  char buffer[MAXHOSTNAMELEN + 1];
  if (gethostname(buffer, MAXHOSTNAMELEN))
    return Qnil;
  buffer[MAXHOSTNAMELEN] = '\000';
  return rb_str_new2(buffer);
#else
  return Qnil;
#endif
}
#endif

VALUE
dnssd_register_new(VALUE service, DNSServiceFlags flags, const char *name,
    const char *regtype, const char *domain) {
  VALUE self = reply_new(service, flags);
  reply_add_names(self, name, regtype, domain);
  /* HACK */
  /* See HACK in dnssd_service.c */
  rb_ivar_set(self, dnssd_iv_interface, rb_ivar_get(service, dnssd_iv_interface));
  rb_ivar_set(self, dnssd_iv_port, rb_ivar_get(service, dnssd_iv_port));
  rb_ivar_set(self, dnssd_iv_text_record, rb_ivar_get(service, dnssd_iv_text_record));
  /********/
  return self;
}

VALUE
dnssd_resolve_new(VALUE service, DNSServiceFlags flags, uint32_t interface,
    const char *fullname, const char *host_target,
    uint16_t opaqueport, uint16_t txt_len, const char *txt_rec) {
  uint16_t port = ntohs(opaqueport);
  VALUE self = reply_new(service, flags);
  reply_set_interface(self, interface);
  reply_add_names2(self, fullname);
  rb_ivar_set(self, dnssd_iv_target, rb_str_new2(host_target));
  rb_ivar_set(self, dnssd_iv_port, UINT2NUM(port));
  reply_set_tr(self, txt_len, txt_rec);
  return self;
}

/*
 * call-seq:
 *    DNSSD::Reply.new() => raises a RuntimeError
 *
 */
static VALUE
reply_initialize(int argc, VALUE *argv, VALUE reply) {
  dnssd_instantiation_error(rb_obj_classname(reply));
  return Qnil;
}

void
Init_DNSSD_Replies(void) {
  int i;
  VALUE flags_hash;
  /* hack so rdoc documents the project correctly */
#ifdef mDNSSD_RDOC_HACK
  mDNSSD = rb_define_module("DNSSD");
#endif

  dnssd_iv_flags = rb_intern("@flags");
  dnssd_iv_interface = rb_intern("@interface");
  dnssd_iv_fullname = rb_intern("@fullname");
  dnssd_iv_target = rb_intern("@target");
  dnssd_iv_port = rb_intern("@port");
  dnssd_iv_text_record = rb_intern("@text_record");
  dnssd_iv_name = rb_intern("@name");
  dnssd_iv_type = rb_intern("@type");
  dnssd_iv_domain = rb_intern("@domain");
  dnssd_iv_service = rb_intern("@service");

  cDNSSDReply = rb_define_class_under(mDNSSD, "Reply", rb_cObject);
  /* DNSSD::Reply objects can only be instantiated by
   * DNSSD.browse(), DNSSD.register(), DNSSD.resolve(), DNSSD.enumerate_domains(). */
  rb_define_method(cDNSSDReply, "initialize", reply_initialize, -1);

  cDNSSDFlags = rb_define_class_under(mDNSSD, "Flags", rb_cObject);

  /* flag constants */
#if DNSSD_MAX_FLAGS != 9
#error The code below needs to be updated.
#endif

  /* MoreComing indicates that at least one more result is queued and will be
   * delivered following immediately after this one.
   *
   * Applications should not update their UI to display browse results when the
   * MoreComing flag is set, because this would result in a great deal of ugly
   * flickering on the screen.  Applications should instead wait until
   * MoreComing is not set, and then update their UI.
   *
   * When MoreComing is not set, that doesn't mean there will be no more
   * answers EVER, just that there are no more answers immediately available
   * right now at this instant.  If more answers become available in the future
   * they will be delivered as usual.
   */
  rb_define_const(cDNSSDFlags, "MoreComing",
		  ULONG2NUM(kDNSServiceFlagsMoreComing));

  /* Applies only to enumeration.  An enumeration callback with the
   * DNSSD::Flags::Add flag NOT set indicates a DNSSD::Flags::Remove, i.e. the
   * domain is no longer valid.
   */
  rb_define_const(cDNSSDFlags, "Add", ULONG2NUM(kDNSServiceFlagsAdd));

  /* Applies only to enumeration and is only valid in conjunction with Add
   */
  rb_define_const(cDNSSDFlags, "Default", ULONG2NUM(kDNSServiceFlagsDefault));

  /* Flag for specifying renaming behavior on name conflict when registering
   * non-shared records.
   *
   * By default, name conflicts are automatically handled by renaming the
   * service.  DNSSD::Flags::NoAutoRename overrides this behavior - with this
   * flag set, name conflicts will result in a callback.  The NoAutoRename flag
   * is only valid if a name is explicitly specified when registering a service
   * (ie the default name is not used.)
   */
  rb_define_const(cDNSSDFlags, "NoAutoRename",
		  ULONG2NUM(kDNSServiceFlagsNoAutoRename));

  /* Flag for registering individual records on a connected DNSServiceRef.
   *
   * DNSSD::Flags::Shared indicates that there may be multiple records with
   * this name on the network (e.g. PTR records).  DNSSD::Flags::Unique
   * indicates that the record's name is to be unique on the network (e.g. SRV
   * records).  (DNSSD::Flags::Shared and DNSSD::Flags::Unique are currently
   * not used by the Ruby API.)
   */
  rb_define_const(cDNSSDFlags, "Shared", ULONG2NUM(kDNSServiceFlagsShared));
  rb_define_const(cDNSSDFlags, "Unique", ULONG2NUM(kDNSServiceFlagsUnique));

  /* DNSSD::Flags::BrowseDomains enumerates domains recommended for browsing
   */
  rb_define_const(cDNSSDFlags, "BrowseDomains",
		  ULONG2NUM(kDNSServiceFlagsBrowseDomains));

  /* DNSSD::Flags::RegistrationDomains enumerates domains recommended for
   * registration.
   */

  rb_define_const(cDNSSDFlags, "RegistrationDomains",
		  ULONG2NUM(kDNSServiceFlagsRegistrationDomains));

  /* Flag for creating a long-lived unicast query for the DNSDS.query_record()
   * (currently not part of the Ruby API). */
  rb_define_const(cDNSSDFlags, "LongLivedQuery",
		  ULONG2NUM(kDNSServiceFlagsLongLivedQuery));

  flags_hash = rb_hash_new();

  for (i = 0; i < DNSSD_MAX_FLAGS; i++) {
    rb_hash_aset(flags_hash, rb_str_new2(dnssd_flag_name[i]),
		 ULONG2NUM(dnssd_flag[i]));
  }

  rb_define_const(cDNSSDFlags, "FLAGS", flags_hash);
}

