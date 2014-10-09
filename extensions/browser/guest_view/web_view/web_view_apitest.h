// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "extensions/browser/guest_view/test_guest_view_manager.h"
#include "extensions/shell/test/shell_test.h"
#include "ui/gfx/switches.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class TestGuestViewManager;

// Base class for WebView tests in app_shell.
class WebViewAPITest : public AppShellTest {
 protected:
  WebViewAPITest();

  // Launches the app_shell app in |app_location|.
  void LaunchApp(const std::string& app_location);

  // Runs the test |test_name| in |app_location|. RunTest will launch the app
  // and execute the javascript function runTest(test_name) inside the app.
  void RunTest(const std::string& test_name, const std::string& app_location);

  // Starts/Stops the embedded test server.
  void StartTestServer();
  void StopTestServer();

  // Returns the GuestViewManager singleton.
  TestGuestViewManager* GetGuestViewManager();

  // content::BrowserTestBase implementation.
  virtual void RunTestOnMainThreadLoop() override;
  virtual void SetUpOnMainThread() override;
  virtual void TearDownOnMainThread() override;

 protected:
  content::WebContents* embedder_web_contents_;
  TestGuestViewManagerFactory factory_;
  base::DictionaryValue test_config_;
};

class WebViewDPIAPITest : public WebViewAPITest {
 protected:
  virtual void SetUp() override;
  static float scale() { return 2.0f; }
};

}  // namespace extensions
