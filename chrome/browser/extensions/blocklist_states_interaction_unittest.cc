// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "components/safe_browsing/buildflags.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_state_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

// The interaction tests rely on the safe-browsing database.
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kTestExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

}  // namespace

// Test suite to test the interaction between Safe Browsing blocklist, Omaha
// attributes blocklist and user action. These tests verify that the extension
// is in the correct extension set under different circumstances.
class BlocklistStatesInteractionUnitTest : public ExtensionServiceTestBase {
 public:
  BlocklistStatesInteractionUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kDisablePolicyViolationExtensionsRemotely);
  }

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeGoodInstalledExtensionService();
    test_blocklist_.Attach(service()->blocklist_);
    service()->Init();
    extension_prefs_ = ExtensionPrefs::Get(profile());
  }

 protected:
  void SetSafeBrowsingBlocklistStateForExtension(
      const std::string& extension_id,
      BlocklistState state) {
    // Reset cache in blocklist to make sure the latest blocklist state is
    // fetched.
    service()->blocklist_->ResetBlocklistStateCacheForTest();
    test_blocklist_.SetBlocklistState(extension_id, state, true);
    task_environment()->RunUntilIdle();
  }

  void SetOmahaBlocklistStateForExtension(const std::string& extension_id,
                                          const std::string& omaha_attribute,
                                          bool value) {
    base::Value attributes(base::Value::Type::DICTIONARY);
    attributes.SetBoolKey(omaha_attribute, value);
    service()->PerformActionBasedOnOmahaAttributes(extension_id, attributes);
  }

  ExtensionPrefs* extension_prefs() { return extension_prefs_; }

 private:
  TestBlocklist test_blocklist_;
  ExtensionPrefs* extension_prefs_;
};

// 1. The extension is added to the Safe Browsing blocklist with
// BLOCKLISTED_MALWARE state.
// 2. The extension is added to the Omaha attribute blocklist with _malware
// attribute.
// 3. The extension is removed from the Safe Browsing blocklist.
// 4. The extension is removed from the Omaha attribute blocklist.
TEST_F(BlocklistStatesInteractionUnitTest,
       SafeBrowsingMalwareThenOmahaAttributeMalware) {
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_MALWARE);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", true);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  // kTestExtensionId should be kept in `blocklisted_extensions` because it is
  // still in the Omaha attribute blocklist.
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  // kTestExtensionId should be removed from the `blocklisted_extensions` and is
  // re-enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
}

// 1. The extension is added to the Omaha attribute blocklist with _malware
// attribute.
// 2. The extension is added to the Safe Browsing blocklist with
// BLOCKLISTED_MALWARE state.
// 3. The extension is removed from the Omaha attribute blocklist.
// 4. The extension is removed from the Safe Browsing blocklist.
TEST_F(BlocklistStatesInteractionUnitTest,
       OmahaAttributeMalwareThenSafeBrowsingMalware) {
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", true);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_MALWARE);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  // TODO(crbug.com/1193695): Ideally this extension should still be blocklisted
  // because the extension is still in the Safe Browsing blocklist.
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  // The extension is added back to the blocklist after the Safe Browsing
  // blocklist is refreshed.
  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_MALWARE);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));

  // The extension should be kept in the `blocklisted_extensions` even if the
  // Omaha attribute is still false.
  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  // kTestExtensionId should be removed from `blocklisted_extensions` and is
  // re-enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
}

// 1. The extension is added to the Safe Browsing greylist with
// BLOCKLISTED_POTENTIALLY_UNWANTED state.
// 2. The extension is added to the Omaha attribute blocklist with _malware
// attribute.
// 3. The extension is removed from the Omaha attribute blocklist.
// 4. The extension is removed from the Safe Browsing greylist.
TEST_F(BlocklistStatesInteractionUnitTest,
       SafeBrowsingUwSThenOmahaAttributeMalware) {
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_POTENTIALLY_UNWANTED);
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", true);
  EXPECT_EQ(BLOCKLISTED_MALWARE,
            extension_prefs()->GetExtensionBlocklistState(kTestExtensionId));
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));
  EXPECT_TRUE(extension_prefs()->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  // TODO(crbug.com/1193695): Ideally this should be set to the original Safe
  // Browsing greylist state BLOCKLISTED_POTENTIALLY_UNWANTED. However, this is
  // not possible with the current implementation, because the Omaha blocklist
  // state (malware) overrides the Safe Browsing blocklist state, and there is
  // no way to preserve the original Safe Browsing greylist state (potentially
  // unwanted). This should happen pretty rare - only when the extension is
  // removed from the Omaha attribute blocklist but stays in the Safe Browsing
  // greylist. It will be fixed after we decouple Safe Browsing blocklist state
  // and Omaha attribute blocklist state.
  EXPECT_EQ(NOT_BLOCKLISTED,
            extension_prefs()->GetExtensionBlocklistState(kTestExtensionId));
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  // The Safe Browsing greylist state should be set correctly after the Safe
  // Browsing blocklist is refreshed.
  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_POTENTIALLY_UNWANTED);
  EXPECT_EQ(BLOCKLISTED_POTENTIALLY_UNWANTED,
            extension_prefs()->GetExtensionBlocklistState(kTestExtensionId));

  // The extension should be kept disabled because it's still in the Safe
  // Browsing greylist.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
}

// 1. The extension is added to the Safe Browsing blocklist with
// BLOCKLISTED_MALWARE state.
// 2. The extension is added to the Omaha attribute greylist with
// _policy_violation attribute.
// 3. The extension is removed from the Safe Browsing blocklist.
// 4. The extension is removed from the Omaha attribute greylist.
TEST_F(BlocklistStatesInteractionUnitTest,
       SafeBrowsingMalwareThenOmahaAttributePolicyViolation) {
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_MALWARE);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));
  EXPECT_EQ(BLOCKLISTED_MALWARE,
            extension_prefs()->GetExtensionBlocklistState(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     true);
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs()));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  // The extension should be kept disabled because it's still in the Omaha
  // attribute greylist.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
  EXPECT_EQ(NOT_BLOCKLISTED,
            extension_prefs()->GetExtensionBlocklistState(kTestExtensionId));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs()));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     false);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
}

// 1. The extension is added to the Safe Browsing greylist with
// BLOCKLISTED_CWS_POLICY_VIOLATION state.
// 2. The extension is added to the Omaha attribute greylist with
// _policy_violation attribute.
// 3. The extension is removed from the Safe Browsing greylist.
// 4. The extension is removed from the Omaha attribute greylist.
TEST_F(BlocklistStatesInteractionUnitTest,
       SafeBrowsingPolicyViolationThenOmahaAttributePolicyViolation) {
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_CWS_POLICY_VIOLATION);
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     true);
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  // The extension should be kept disabled because it's still in the Omaha
  // attribute greylist.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     false);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
}

// 1. The extension is added to the Omaha attribute greylist with
// BLOCKLISTED_CWS_POLICY_VIOLATION state.
// 2. The extension is added to the Safe Browsing greylist with
// _policy_violation attribute.
// 3. The extension is removed from the Omaha attribute greylist.
// 4. The extension is removed from the Safe Browsing greylist.
TEST_F(BlocklistStatesInteractionUnitTest,
       OmahaAttributePolicyViolationThenSafeBrowsingPolicyViolation) {
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     true);
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_CWS_POLICY_VIOLATION);
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     false);
  // The extension should be kept disabled because it's still in the Safe
  // Browsing greylist.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
}

// 1. The extension is added to the Safe Browsing greylist with
// BLOCKLISTED_CWS_POLICY_VIOLATION state.
// 2. User re-enabled the extension.
// 3. The extension is added to the Omaha attribute greylist with
// _policy_violation attribute.
// 4. The extension is removed from the Safe Browsing greylist.
// 5. The extension is removed from the Omaha attribute greylist.
TEST_F(
    BlocklistStatesInteractionUnitTest,
    SafeBrowsingPolicyViolationThenOmahaAttributePolicyViolationWithUserAction) {
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_CWS_POLICY_VIOLATION);
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs()));

  // The extension is manually re-enabled.
  service()->EnableExtension(kTestExtensionId);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     true);
  // The extension is not disabled again, because it was previously manually
  // re-enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  // The acknowledged state should not be cleared yet, because it is still in
  // the Omaha attribute greylist.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs()));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_policy_violation",
                                     false);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  // The acknowledged state should be removed now.
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs()));
}

}  // namespace extensions

#endif
