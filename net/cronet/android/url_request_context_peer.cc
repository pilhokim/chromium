// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cronet/android/url_request_context_peer.h"

#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace {

class BasicNetworkDelegate : public net::NetworkDelegate {
 public:
  BasicNetworkDelegate() {}
  virtual ~BasicNetworkDelegate() {}

 private:
  // net::NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE {
    return net::OK;
  }

  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE {
    return net::OK;
  }

  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE {}

  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* _response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE {
    return net::OK;
  }

  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE {}

  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {}

  virtual void OnRawBytesRead(const net::URLRequest& request,
                              int bytes_read) OVERRIDE {}

  virtual void OnCompleted(net::URLRequest* request, bool started) OVERRIDE {}

  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE {}

  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) OVERRIDE {}

  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE {
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  virtual bool OnCanGetCookies(const net::URLRequest& request,
                               const net::CookieList& cookie_list) OVERRIDE {
    return false;
  }

  virtual bool OnCanSetCookie(const net::URLRequest& request,
                              const std::string& cookie_line,
                              net::CookieOptions* options) OVERRIDE {
    return false;
  }

  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const OVERRIDE {
    return false;
  }

  virtual bool OnCanThrottleRequest(const net::URLRequest& request)
      const OVERRIDE {
    return false;
  }

  virtual int OnBeforeSocketStreamConnect(
      net::SocketStream* stream,
      const net::CompletionCallback& callback) OVERRIDE {
    return net::OK;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

class BasicURLRequestContext : public net::URLRequestContext {
 public:
  BasicURLRequestContext() : storage_(this) {}

  net::URLRequestContextStorage* storage() { return &storage_; }

 protected:
  virtual ~BasicURLRequestContext() {}

 private:
  net::URLRequestContextStorage storage_;

  DISALLOW_COPY_AND_ASSIGN(BasicURLRequestContext);
};

}  // namespace

URLRequestContextPeer::URLRequestContextPeer(
    URLRequestContextPeerDelegate* delegate,
    std::string user_agent,
    int logging_level,
    const char* version) {
  delegate_ = delegate;
  user_agent_ = user_agent;
  logging_level_ = logging_level;
  version_ = version;
}

void URLRequestContextPeer::Initialize() {
  network_thread_ = new base::Thread("network");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  network_thread_->StartWithOptions(options);

  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestContextPeer::InitializeURLRequestContext, this));
}

void URLRequestContextPeer::InitializeURLRequestContext() {
  BasicURLRequestContext* context = new BasicURLRequestContext;
  net::URLRequestContextStorage* storage = context->storage();

  net::NetworkDelegate* network_delegate = new BasicNetworkDelegate;
  storage->set_network_delegate(network_delegate);

  storage->set_host_resolver(net::HostResolver::CreateDefaultResolver(NULL));

  net::ProxyConfigService* proxy_config_service =
      new net::ProxyConfigServiceFixed(net::ProxyConfig());
  storage->set_proxy_service(net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service,
      4,  // TODO(willchan): Find a better constant somewhere.
      context->net_log()));
  storage->set_ssl_config_service(new net::SSLConfigServiceDefaults);
  storage->set_http_auth_handler_factory(
      net::HttpAuthHandlerRegistryFactory::CreateDefault(
          context->host_resolver()));
  storage->set_transport_security_state(new net::TransportSecurityState());
  storage->set_http_server_properties(scoped_ptr<net::HttpServerProperties>(
      new net::HttpServerPropertiesImpl()));
  storage->set_cert_verifier(net::CertVerifier::CreateDefault());

  net::HttpNetworkSession::Params network_session_params;
  network_session_params.host_resolver = context->host_resolver();
  network_session_params.cert_verifier = context->cert_verifier();
  network_session_params.transport_security_state =
      context->transport_security_state();
  network_session_params.proxy_service = context->proxy_service();
  network_session_params.ssl_config_service = context->ssl_config_service();
  network_session_params.http_auth_handler_factory =
      context->http_auth_handler_factory();
  network_session_params.network_delegate = network_delegate;
  network_session_params.http_server_properties =
      context->http_server_properties();
  network_session_params.net_log = context->net_log();

  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(network_session_params));

  net::HttpTransactionFactory* http_transaction_factory =
      new net::HttpNetworkLayer(network_session.get());
  storage->set_http_transaction_factory(http_transaction_factory);

  net::URLRequestJobFactoryImpl* job_factory =
      new net::URLRequestJobFactoryImpl;
  storage->set_job_factory(job_factory);

  context_.reset(context);

  if (VLOG_IS_ON(2)) {
    context_->set_net_log(new net::NetLog);
    netlog_observer_.reset(new NetLogObserver(logging_level_));
    context_->net_log()->AddThreadSafeObserver(netlog_observer_.get(),
                                               net::NetLog::LOG_ALL_BUT_BYTES);
  }

  net::HttpStreamFactory::EnableNpnSpdy31();

  delegate_->OnContextInitialized(this);
}

URLRequestContextPeer::~URLRequestContextPeer() {}

const std::string& URLRequestContextPeer::GetUserAgent(const GURL& url) const {
  return user_agent_;
}

net::URLRequestContext* URLRequestContextPeer::GetURLRequestContext() {
  if (!context_) {
    LOG(ERROR) << "URLRequestContext is not set up";
  }
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextPeer::GetNetworkTaskRunner() const {
  return network_thread_->message_loop_proxy();
}

void NetLogObserver::OnAddEntry(const net::NetLog::Entry& entry) {
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "Net log entry: type=" << entry.type()
            << ", source=" << entry.source().type
            << ", phase=" << entry.phase();
  }
}
