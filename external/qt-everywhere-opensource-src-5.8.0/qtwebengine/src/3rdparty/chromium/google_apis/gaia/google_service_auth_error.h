// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A GoogleServiceAuthError is immutable, plain old data representing an
// error from an attempt to authenticate with a Google service.
// It could be from Google Accounts itself, or any service using Google
// Accounts (e.g expired credentials).  It may contain additional data such as
// captcha or OTP challenges.

// A GoogleServiceAuthError without additional data is just a State, defined
// below. A case could be made to have this relation implicit, to allow raising
// error events concisely by doing OnAuthError(GoogleServiceAuthError::NONE),
// for example. But the truth is this class is ever so slightly more than a
// transparent wrapper around 'State' due to additional Captcha data
// (e.g consider operator=), and this would violate the style guide. Thus,
// you must explicitly use the constructor when all you have is a State.
// The good news is the implementation nests the enum inside a class, so you
// may forward declare and typedef GoogleServiceAuthError to something shorter
// in the comfort of your own translation unit.

#ifndef GOOGLE_APIS_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_
#define GOOGLE_APIS_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_

#include <string>

#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

class GoogleServiceAuthError {
 public:
  //
  // These enumerations are referenced by integer value in HTML login code and
  // in UMA histograms. Do not change the numeric values.
  //
  enum State {
    // The user is authenticated.
    NONE = 0,

    // The credentials supplied to GAIA were either invalid, or the locally
    // cached credentials have expired.
    INVALID_GAIA_CREDENTIALS = 1,

    // The GAIA user is not authorized to use the service.
    USER_NOT_SIGNED_UP = 2,

    // Could not connect to server to verify credentials. This could be in
    // response to either failure to connect to GAIA or failure to connect to
    // the service needing GAIA tokens during authentication.
    CONNECTION_FAILED = 3,

    // The user needs to satisfy a CAPTCHA challenge to unlock their account.
    // If no other information is available, this can be resolved by visiting
    // https://accounts.google.com/DisplayUnlockCaptcha. Otherwise, captcha()
    // will provide details about the associated challenge.
    CAPTCHA_REQUIRED = 4,

    // The user account has been deleted.
    ACCOUNT_DELETED = 5,

    // The user account has been disabled.
    ACCOUNT_DISABLED = 6,

    // The service is not available; try again later.
    SERVICE_UNAVAILABLE = 7,

    // The password is valid but we need two factor to get a token.
    TWO_FACTOR = 8,

    // The requestor of the authentication step cancelled the request
    // prior to completion.
    REQUEST_CANCELED = 9,

    // The user has provided a HOSTED account, when this service requires
    // a GOOGLE account.
    HOSTED_NOT_ALLOWED = 10,

    // Indicates the service responded to a request, but we cannot
    // interpret the response.
    UNEXPECTED_SERVICE_RESPONSE = 11,

    // Indicates the service responded and response carried details of the
    // application error.
    SERVICE_ERROR = 12,

    // The password is valid but web login is required to get a token.
    WEB_LOGIN_REQUIRED = 13,

    // The number of known error states.
    NUM_STATES = 14,
  };

  // Additional data for CAPTCHA_REQUIRED errors.
  struct Captcha {
    Captcha();
    Captcha(const std::string& token,
            const GURL& audio,
            const GURL& img,
            const GURL& unlock,
            int width,
            int height);
    Captcha(const Captcha& other);
    ~Captcha();
    // For test only.
    bool operator==(const Captcha &b) const;

    std::string token;  // Globally identifies the specific CAPTCHA challenge.
    GURL audio_url;     // The CAPTCHA audio to use instead of image.
    GURL image_url;     // The CAPTCHA image to show the user.
    GURL unlock_url;    // Pretty unlock page containing above captcha.
    int image_width;    // Width of captcha image.
    int image_height;   // Height of capture image.
  };

  // Additional data for TWO_FACTOR errors.
  struct SecondFactor {
    SecondFactor();
    SecondFactor(const std::string& token,
                 const std::string& prompt,
                 const std::string& alternate,
                 int length);
    SecondFactor(const SecondFactor& other);
    ~SecondFactor();
    // For test only.
    bool operator==(const SecondFactor &b) const;

    // Globally identifies the specific second-factor challenge.
    std::string token;
    // Localised prompt text, eg Enter the verification code sent to your
    // phone number ending in XXX.
    std::string prompt_text;
    // Localized text describing an alternate option, eg Get a verification
    // code in a text message.
    std::string alternate_text;
    // Character length for the challenge field.
    int field_length;
  };

  // For test only.
  bool operator==(const GoogleServiceAuthError &b) const;
  bool operator!=(const GoogleServiceAuthError &b) const;

  // Construct a GoogleServiceAuthError from a State with no additional data.
  explicit GoogleServiceAuthError(State s);

  GoogleServiceAuthError(const GoogleServiceAuthError& other);

  // Construct a GoogleServiceAuthError from a network error.
  // It will be created with CONNECTION_FAILED set.
  static GoogleServiceAuthError FromConnectionError(int error);

  // Construct a CAPTCHA_REQUIRED error with CAPTCHA challenge data from the
  // the ClientLogin endpoint.
  // TODO(rogerta): once ClientLogin is no longer used, may be able to get
  // rid of this function.
  static GoogleServiceAuthError FromClientLoginCaptchaChallenge(
      const std::string& captcha_token,
      const GURL& captcha_image_url,
      const GURL& captcha_unlock_url);

  // Construct a SERVICE_ERROR error, e.g. invalid client ID, with an
  // |error_message| which provides more information about the service error.
  static GoogleServiceAuthError FromServiceError(
      const std::string& error_message);

  // Construct an UNEXPECTED_SERVICE_RESPONSE error, with an |error_message|
  // detailing the problems with the response.
  static GoogleServiceAuthError FromUnexpectedServiceResponse(
      const std::string& error_message);

  // Provided for convenience for clients needing to reset an instance to NONE.
  // (avoids err_ = GoogleServiceAuthError(GoogleServiceAuthError::NONE), due
  // to explicit class and State enum relation. Note: shouldn't be inlined!
  static GoogleServiceAuthError AuthErrorNone();

  // The error information.
  State state() const;
  const Captcha& captcha() const;
  const SecondFactor& second_factor() const;
  int network_error() const;
  const std::string& token() const;
  const std::string& error_message() const;

  // Returns info about this object in a dictionary.  Caller takes
  // ownership of returned dictionary.
  base::DictionaryValue* ToValue() const;

  // Returns a message describing the error.
  std::string ToString() const;

  // Check if this is error may go away simply by trying again.  Except for the
  // NONE case, these are mutually exclusive.
  bool IsPersistentError() const;
  bool IsTransientError() const;

 private:
  GoogleServiceAuthError(State s, int error);

  // Construct a GoogleServiceAuthError from |state| and |error_message|.
  GoogleServiceAuthError(State state, const std::string& error_message);

  GoogleServiceAuthError(State s, const std::string& captcha_token,
                         const GURL& captcha_audio_url,
                         const GURL& captcha_image_url,
                         const GURL& captcha_unlock_url,
                         int image_width,
                         int image_height);

  State state_;
  Captcha captcha_;
  SecondFactor second_factor_;
  int network_error_;
  std::string error_message_;
};

#endif  // GOOGLE_APIS_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_
