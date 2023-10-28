// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_DOCUMENT_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_DOCUMENT_SUGGESTIONS_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class DocumentSuggestionsService;
class Profile;

class DocumentSuggestionsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static DocumentSuggestionsService* GetForProfile(Profile* profile,
                                                   bool create_if_necessary);
  static DocumentSuggestionsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DocumentSuggestionsServiceFactory>;

  DocumentSuggestionsServiceFactory();
  ~DocumentSuggestionsServiceFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(DocumentSuggestionsServiceFactory);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_DOCUMENT_SUGGESTIONS_SERVICE_FACTORY_H_
