// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_H_

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_model.h"

namespace autofill {

// A midline horizontal ellipsis (U+22EF).
extern const base::char16 kMidlineEllipsis[];

// A form group that stores credit card information.
class CreditCard : public AutofillDataModel {
 public:
  enum RecordType {
    // A card with a complete number managed by Chrome (and not representing
    // something on the server).
    LOCAL_CARD,

    // A card from Wallet with masked information. Such cards will only have
    // the last 4 digits of the card number, and require an extra download to
    // convert to a FULL_SERVER_CARD.
    MASKED_SERVER_CARD,

    // A card from the Wallet server with full information. This card is not
    // locally editable.
    FULL_SERVER_CARD,
  };

  // The status of this credit card. Only used for server cards.
  enum ServerStatus {
    EXPIRED,
    OK,
  };

  CreditCard(const std::string& guid, const std::string& origin);
  CreditCard(const base::string16& card_number,
             int expiration_month,
             int expiration_year);

  // Creates a server card.  The type must be MASKED_SERVER_CARD or
  // FULL_SERVER_CARD.
  CreditCard(RecordType type, const std::string& server_id);

  // For use in STL containers.
  CreditCard();
  CreditCard(const CreditCard& credit_card);
  ~CreditCard() override;

  // Returns a version of |number| that has any separator characters removed.
  static const base::string16 StripSeparators(const base::string16& number);

  // The user-visible type of the card, e.g. 'Mastercard'.
  static base::string16 TypeForDisplay(const std::string& type);

// This method is not compiled on iOS because the resources are not used and
// should not be shipped.
// TODO(jdonnelly): Use credit card issuer images on iOS.
// http://crbug.com/535784
#if !defined(OS_IOS)
  // The ResourceBundle ID for the appropriate credit card image.
  static int IconResourceId(const std::string& type);
#endif  // #if !defined(OS_IOS)

  // Returns the internal representation of credit card type corresponding to
  // the given |number|.  The credit card type is determined purely according to
  // the Issuer Identification Number (IIN), a.k.a. the "Bank Identification
  // Number (BIN)", which is parsed from the relevant prefix of the |number|.
  // This function performs no additional validation checks on the |number|.
  // Hence, the returned type for both the valid card "4111-1111-1111-1111" and
  // the invalid card "4garbage" will be Visa, which has an IIN of 4.
  static const char* GetCreditCardType(const base::string16& number);

  // Type strings are defined at the bottom of this file, e.g. kVisaCard.
  void SetTypeForMaskedCard(const char* type);

  // Sets/gets the status of a server card.
  void SetServerStatus(ServerStatus status);
  ServerStatus GetServerStatus() const;

  // FormGroup:
  void GetMatchingTypes(const base::string16& text,
                        const std::string& app_locale,
                        ServerFieldTypeSet* matching_types) const override;
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfo(ServerFieldType type, const base::string16& value) override;
  base::string16 GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  bool SetInfo(const AutofillType& type,
               const base::string16& value,
               const std::string& app_locale) override;

  // Credit card preview summary, for example: "Visa - 1234", ", 01/2020".
  const std::pair<base::string16, base::string16> LabelPieces() const;

  // Like LabelPieces, but appends the two pieces together.
  const base::string16 Label() const;

  // Special method to set value for HTML5 month input type.
  void SetInfoForMonthInputType(const base::string16& value);

  // The last four digits of the credit card number (or possibly less if there
  // aren't enough characters).
  base::string16 LastFourDigits() const;
  // The user-visible type of the card, e.g. 'Mastercard'.
  base::string16 TypeForDisplay() const;
  // A label for this credit card formatted as 'Cardname - 2345'.
  base::string16 TypeAndLastFourDigits() const;

  // Localized expiration for this credit card formatted as 'Exp: 06/17'.
  base::string16 AbbreviatedExpirationDateForDisplay() const;

  const std::string& type() const { return type_; }

  int expiration_month() const { return expiration_month_; }
  int expiration_year() const { return expiration_year_; }

  // These setters verify that the month and year are within appropriate
  // ranges, or 0. They take integers as an alternative to setting the inputs
  // from strings via SetInfo().
  void SetExpirationMonth(int expiration_month);
  void SetExpirationYear(int expiration_year);

  const std::string& server_id() const { return server_id_; }
  void set_server_id(const std::string& server_id) { server_id_ = server_id; }

  // For use in STL containers.
  void operator=(const CreditCard& credit_card);

  // If the card numbers for |this| and |imported_card| match, and merging the
  // two wouldn't result in unverified data overwriting verified data,
  // overwrites |this| card's data with the data in |credit_card|.
  // Returns true if the card numbers match, false otherwise.
  bool UpdateFromImportedCard(const CreditCard& imported_card,
                              const std::string& app_locale) WARN_UNUSED_RESULT;

  // Comparison for Sync.  Returns 0 if the credit card is the same as |this|,
  // or < 0, or > 0 if it is different.  The implied ordering can be used for
  // culling duplicates.  The ordering is based on collation order of the
  // textual contents of the fields.
  // GUIDs, origins, labels, and unique IDs are not compared, only the values of
  // the credit cards themselves.
  int Compare(const CreditCard& credit_card) const;

  // Determines if |this| is a local version of the server card |other|.
  bool IsLocalDuplicateOfServerCard(const CreditCard& other) const;

  // Determines if |this| has the same number as |other|. If either is a masked
  // server card, compares the last four digits only.
  bool HasSameNumberAs(const CreditCard& other) const;

  // Equality operators compare GUIDs, origins, and the contents.
  // Usage metadata (use count, use date, modification date) are NOT compared.
  bool operator==(const CreditCard& credit_card) const;
  bool operator!=(const CreditCard& credit_card) const;

  // How this card is stored.
  RecordType record_type() const { return record_type_; }
  void set_record_type(RecordType rt) { record_type_ = rt; }

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Returns true if all field types have valid values set and the card is not
  // expired. MASKED_SERVER_CARDs will never be valid because the number is
  // not complete.
  bool IsValid() const;

  // Returns the credit card number.
  const base::string16& number() const { return number_; }
  // Sets |number_| to |number| and computes the appropriate card |type_|.
  void SetNumber(const base::string16& number);

  // Logs the number of days since the credit card was last used and records its
  // use.
  void RecordAndLogUse();

  // Converts a string representation of a month (such as "February" or "feb."
  // or "2") into a numeric value in [1, 12]. Returns true on successful
  // conversion or false if a month was not recognized.
  static bool ConvertMonth(const base::string16& month,
                           const std::string& app_locale,
                           int* num);

  // Returns whether the credit card is expired based on |current_time|.
  bool IsExpired(const base::Time& current_time) const;

  // Whether the card expiration date should be updated.
  bool ShouldUpdateExpiration(const base::Time& current_time) const;

  const std::string& billing_address_id() const { return billing_address_id_; }
  void set_billing_address_id(const std::string& id) {
    billing_address_id_ = id;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(CreditCardTest, SetExpirationDateFromString);
  FRIEND_TEST_ALL_PREFIXES(CreditCardTest, SetExpirationYearFromString);

  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

  // The type of the card to fill in to the page, e.g. 'Mastercard'.
  base::string16 TypeForFill() const;

  // The month and year are zero if not present.
  int Expiration4DigitYear() const { return expiration_year_; }
  int Expiration2DigitYear() const { return expiration_year_ % 100; }
  base::string16 ExpirationMonthAsString() const;
  base::string16 Expiration4DigitYearAsString() const;
  base::string16 Expiration2DigitYearAsString() const;

  // Sets |expiration_month_| to the integer conversion of |text| and returns
  // whether the operation was successful.
  bool SetExpirationMonthFromString(const base::string16& text,
                                    const std::string& app_locale);

  // Sets |expiration_year_| to the integer conversion of |text|. Will handle
  // 4-digit year or 2-digit year (eventually converted to 4-digit year).
  void SetExpirationYearFromString(const base::string16& text);

  // Sets |expiration_year_| and |expiration_month_| to the integer conversion
  // of |text|. Will handle mmyy, mmyyyy, mm-yyyy and mm-yy as well as single
  // digit months, with various separators.
  void SetExpirationDateFromString(const base::string16& text);

  // See enum definition above.
  RecordType record_type_;

  // The credit card number. For MASKED_SERVER_CARDs, this number will
  // just contain the last four digits of the card number.
  base::string16 number_;

  // The cardholder's name. May be empty.
  base::string16 name_on_card_;

  // The type of the card. This is one of the k...Card constants below.
  std::string type_;

  // These members are zero if not present.
  int expiration_month_;
  int expiration_year_;

  // For server cards (both MASKED and UNMASKED) this is the ID assigned by the
  // server to uniquely identify this card.
  std::string server_id_;

  // The status of the card, as reported by the server. Not valid for local
  // cards.
  ServerStatus server_status_;

  // The identifier of the billing address for this card.
  std::string billing_address_id_;
};

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card);

// The string identifiers for credit card icon resources.
extern const char kAmericanExpressCard[];
extern const char kDinersCard[];
extern const char kDiscoverCard[];
extern const char kGenericCard[];
extern const char kJCBCard[];
extern const char kMasterCard[];
extern const char kUnionPay[];
extern const char kVisaCard[];

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_H_
