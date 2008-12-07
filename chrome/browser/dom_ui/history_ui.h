// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_HISTORY_UI_H__
#define CHROME_BROWSER_DOM_UI_HISTORY_UI_H__

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"

class GURL;

class HistoryUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  HistoryUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUIHTMLSource);
};

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public DOMMessageHandler,
                               public NotificationObserver {
 public:
  explicit BrowsingHistoryHandler(DOMUI* dom_ui_);
  virtual ~BrowsingHistoryHandler();

  // Callback for the "getHistory" message.
  void HandleGetHistory(const Value* value);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Callback from the history system when the most visited list is available.
  void QueryComplete(HistoryService::Handle request_handle,
                     history::QueryResults* results);

  // Extract the arguments from the call to HandleGetHistory.
  void ExtractGetHistoryArguments(const Value* value, 
                                  int* month, 
                                  std::wstring* query);

  // Get the query options for a given month and query.
  history::QueryOptions CreateQueryOptions(int month, 
                                           const std::wstring& query);

  // Current search text.
  std::wstring search_text_;

  // Our consumer for the history service.
  CancelableRequestConsumerT<PageUsageData*, NULL> cancelable_consumer_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

class HistoryUI : public DOMUI {
 public:
  explicit HistoryUI(DOMUIContents* contents);

  // Return the URL for the front page of this UI.
  static GURL GetBaseURL();

  // DOMUI Implementation
  virtual void Init();

 private:
  DOMUIContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(HistoryUI);
};

#endif  // CHROME_BROWSER_DOM_UI_HISTORY_UI_H__