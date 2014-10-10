// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_CLIENT_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/chrome_notification_observer.h"
#include "extensions/browser/extensions_browser_client.h"

namespace base {
class CommandLine;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class ChromeComponentExtensionResourceManager;
class ChromeExtensionsAPIClient;
class ChromeProcessManagerDelegate;
class ContentSettingsPrefsObserver;

// Implementation of extensions::BrowserClient for Chrome, which includes
// knowledge of Profiles, BrowserContexts and incognito.
//
// NOTE: Methods that do not require knowledge of browser concepts should be
// implemented in ChromeExtensionsClient even if they are only used in the
// browser process (see chrome/common/extensions/chrome_extensions_client.h).
class ChromeExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  ChromeExtensionsBrowserClient();
  virtual ~ChromeExtensionsBrowserClient();

  // BrowserClient overrides:
  virtual bool IsShuttingDown() override;
  virtual bool AreExtensionsDisabled(const base::CommandLine& command_line,
                                     content::BrowserContext* context) override;
  virtual bool IsValidContext(content::BrowserContext* context) override;
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) override;
  virtual bool HasOffTheRecordContext(
      content::BrowserContext* context) override;
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;
  virtual bool IsGuestSession(content::BrowserContext* context) const override;
  virtual bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const override;
  virtual bool CanExtensionCrossIncognito(
      const extensions::Extension* extension,
      content::BrowserContext* context) const override;
  virtual net::URLRequestJob* MaybeCreateResourceBundleRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& directory_path,
      const std::string& content_security_policy,
      bool send_cors_header) override;
  virtual bool AllowCrossRendererResourceLoad(net::URLRequest* request,
                                              bool is_incognito,
                                              const Extension* extension,
                                              InfoMap* extension_info_map)
      override;
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override;
  virtual void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<ExtensionPrefsObserver*>* observers) const override;
  virtual ProcessManagerDelegate* GetProcessManagerDelegate() const override;
  virtual scoped_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate()
      override;
  virtual bool DidVersionUpdate(content::BrowserContext* context) override;
  virtual void PermitExternalProtocolHandler() override;
  virtual scoped_ptr<AppSorting> CreateAppSorting() override;
  virtual bool IsRunningInForcedAppMode() override;
  virtual ApiActivityMonitor* GetApiActivityMonitor(
      content::BrowserContext* context) override;
  virtual ExtensionSystemProvider* GetExtensionSystemFactory() override;
  virtual void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) const override;
  virtual scoped_ptr<extensions::RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const override;
  virtual ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() override;
  virtual void BroadcastEventToRenderers(
      const std::string& event_name,
      scoped_ptr<base::ListValue> args) override;
  virtual net::NetLog* GetNetLog() override;
  virtual ExtensionCache* GetExtensionCache() override;

 private:
  friend struct base::DefaultLazyInstanceTraits<ChromeExtensionsBrowserClient>;

  // Observer for Chrome-specific notifications.
  ChromeNotificationObserver notification_observer_;

  // Support for ProcessManager.
  scoped_ptr<ChromeProcessManagerDelegate> process_manager_delegate_;

  // Client for API implementations.
  scoped_ptr<ChromeExtensionsAPIClient> api_client_;

  scoped_ptr<ChromeComponentExtensionResourceManager> resource_manager_;

  scoped_ptr<ExtensionCache> extension_cache_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_CLIENT_H_
