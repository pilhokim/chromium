// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CRONET_ANDROID_URL_REQUEST_CONTEXT_PEER_H_
#define NET_CRONET_ANDROID_URL_REQUEST_CONTEXT_PEER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

// Implementation of the Chromium NetLog observer interface.
class NetLogObserver : public net::NetLog::ThreadSafeObserver {
 public:
  explicit NetLogObserver(int log_level) { log_level_ = log_level; }

  virtual ~NetLogObserver() {}

  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

 private:
  int log_level_;

  DISALLOW_COPY_AND_ASSIGN(NetLogObserver);
};

// Fully configured |URLRequestContext|.
class URLRequestContextPeer : public net::URLRequestContextGetter {
 public:
  class URLRequestContextPeerDelegate
      : public base::RefCountedThreadSafe<URLRequestContextPeerDelegate> {
   public:
    virtual void OnContextInitialized(URLRequestContextPeer* context) = 0;

   protected:
    friend class base::RefCountedThreadSafe<URLRequestContextPeerDelegate>;

    virtual ~URLRequestContextPeerDelegate() {}
  };

  URLRequestContextPeer(URLRequestContextPeerDelegate* delegate,
                        std::string user_agent,
                        int log_level,
                        const char* version);
  void Initialize();

  const std::string& GetUserAgent(const GURL& url) const;

  int logging_level() const { return logging_level_; }

  const char* version() const { return version_; }

  // net::URLRequestContextGetter implementation:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const OVERRIDE;

 private:
  scoped_refptr<URLRequestContextPeerDelegate> delegate_;
  scoped_ptr<net::URLRequestContext> context_;
  int logging_level_;
  const char* version_;
  std::string user_agent_;
  base::Thread* network_thread_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<NetLogObserver> netlog_observer_;

  virtual ~URLRequestContextPeer();

  // Initializes |context_| on the IO thread.
  void InitializeURLRequestContext();

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextPeer);
};

#endif  // NET_CRONET_ANDROID_URL_REQUEST_CONTEXT_PEER_H_
