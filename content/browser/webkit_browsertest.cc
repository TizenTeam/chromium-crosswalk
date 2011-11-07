// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/net/url_request_abort_on_end_job.h"
#include "content/browser/tab_contents/tab_contents.h"

typedef InProcessBrowserTest WebKitBrowserTest;

const char kAsyncScriptThatAbortsOnEndPage[] =
    "files/webkit/async_script_abort_on_end.html";

// This is a browser test because it is hard to reproduce reliably in a
// layout test without races. http://crbug.com/75604 deals with a request
// for an async script which gets data in the response and immediately
// after aborts. This test creates that condition, and it is passed
// if chrome does not crash.

IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, AbortOnEnd) {
  ASSERT_TRUE(test_server()->Start());
  URLRequestAbortOnEndJob::AddUrlHandler();
  GURL url = test_server()->GetURL(kAsyncScriptThatAbortsOnEndPage);

  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* tab_contents = browser()->GetSelectedTabContents();
  // If you are seeing this test fail, please strongly investigate the
  // possibility that http://crbug.com/75604 and
  // https://bugs.webkit.org/show_bug.cgi?id=71122 have reverted before
  // marking this as flakey.
  EXPECT_FALSE(tab_contents->is_crashed());
}

