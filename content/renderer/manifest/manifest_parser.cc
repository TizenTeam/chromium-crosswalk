// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/manifest.h"

namespace content {

namespace {

enum TrimType {
  Trim,
  NoTrim
};

base::NullableString16 ParseString(const base::DictionaryValue& dictionary,
                                   const std::string& key,
                                   TrimType trim) {
  if (!dictionary.HasKey(key))
    return base::NullableString16();

  base::string16 value;
  if (!dictionary.GetString(key, &value)) {
    // TODO(mlamouri): provide a custom message to the developer console about
    // the property being incorrectly set.
    return base::NullableString16();
  }

  if (trim == Trim)
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
  return base::NullableString16(value, false);
}

// Helper function to parse URLs present on a given |dictionary| in a given
// field identified by its |key|. The URL is first parsed as a string then
// resolved using |base_url|.
// Returns a GURL. If the parsing failed, the GURL will not be valid.
GURL ParseURL(const base::DictionaryValue& dictionary,
              const std::string& key,
              const GURL& base_url) {
  base::NullableString16 url_str = ParseString(dictionary, key, NoTrim);
  if (url_str.is_null())
    return GURL();

  return base_url.Resolve(url_str.string());
}

// Parses the 'name' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-name-member
// Returns the parsed string if any, a null string if the parsing failed.
base::NullableString16 ParseName(const base::DictionaryValue& dictionary)  {
  return ParseString(dictionary, "name", Trim);
}

// Parses the 'short_name' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-short-name-member
// Returns the parsed string if any, a null string if the parsing failed.
base::NullableString16 ParseShortName(
    const base::DictionaryValue& dictionary)  {
  return ParseString(dictionary, "short_name", Trim);
}

// Parses the 'start_url' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-start_url-member
// Returns the parsed GURL if any, an empty GURL if the parsing failed.
GURL ParseStartURL(const base::DictionaryValue& dictionary,
                   const GURL& manifest_url,
                   const GURL& document_url) {
  GURL start_url = ParseURL(dictionary, "start_url", manifest_url);
  if (!start_url.is_valid())
    return GURL();

  if (start_url.GetOrigin() != document_url.GetOrigin()) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return GURL();
  }

  return start_url;
}

// Parses the 'display' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-display-member
// Returns the parsed DisplayMode if any, DISPLAY_MODE_UNSPECIFIED if the
// parsing failed.
Manifest::DisplayMode ParseDisplay(const base::DictionaryValue& dictionary) {
  base::NullableString16 display = ParseString(dictionary, "display", Trim);

  if (display.is_null())
    return Manifest::DISPLAY_MODE_UNSPECIFIED;

  if (LowerCaseEqualsASCII(display.string(), "fullscreen"))
    return Manifest::DISPLAY_MODE_FULLSCREEN;
  else if (LowerCaseEqualsASCII(display.string(), "standalone"))
    return Manifest::DISPLAY_MODE_STANDALONE;
  else if (LowerCaseEqualsASCII(display.string(), "minimal-ui"))
    return Manifest::DISPLAY_MODE_MINIMAL_UI;
  else if (LowerCaseEqualsASCII(display.string(), "browser"))
    return Manifest::DISPLAY_MODE_BROWSER;
  else
    return Manifest::DISPLAY_MODE_UNSPECIFIED;
}

// Parses the 'orientation' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-orientation-member
// Returns the parsed WebScreenOrientationLockType if any,
// WebScreenOrientationLockDefault if the parsing failed.
blink::WebScreenOrientationLockType ParseOrientation(
    const base::DictionaryValue& dictionary) {
  base::NullableString16 orientation =
      ParseString(dictionary, "orientation", Trim);

  if (orientation.is_null())
    return blink::WebScreenOrientationLockDefault;

  if (LowerCaseEqualsASCII(orientation.string(), "any"))
    return blink::WebScreenOrientationLockAny;
  else if (LowerCaseEqualsASCII(orientation.string(), "natural"))
    return blink::WebScreenOrientationLockNatural;
  else if (LowerCaseEqualsASCII(orientation.string(), "landscape"))
    return blink::WebScreenOrientationLockLandscape;
  else if (LowerCaseEqualsASCII(orientation.string(), "landscape-primary"))
    return blink::WebScreenOrientationLockLandscapePrimary;
  else if (LowerCaseEqualsASCII(orientation.string(), "landscape-secondary"))
    return blink::WebScreenOrientationLockLandscapeSecondary;
  else if (LowerCaseEqualsASCII(orientation.string(), "portrait"))
    return blink::WebScreenOrientationLockPortrait;
  else if (LowerCaseEqualsASCII(orientation.string(), "portrait-primary"))
    return blink::WebScreenOrientationLockPortraitPrimary;
  else if (LowerCaseEqualsASCII(orientation.string(), "portrait-secondary"))
    return blink::WebScreenOrientationLockPortraitSecondary;
  else
    return blink::WebScreenOrientationLockDefault;
}

// Parses the 'src' field of an icon, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-src-member-of-an-icon
// Returns the parsed GURL if any, an empty GURL if the parsing failed.
GURL ParseIconSrc(const base::DictionaryValue& icon,
                  const GURL& manifest_url) {
  return ParseURL(icon, "src", manifest_url);
}

// Parses the 'type' field of an icon, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-type-member-of-an-icon
// Returns the parsed string if any, a null string if the parsing failed.
base::NullableString16 ParseIconType(const base::DictionaryValue& icon) {
    return ParseString(icon, "type", Trim);
}

// Parses the 'density' field of an icon, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-a-density-member-of-an-icon
// Returns the parsed double if any, Manifest::Icon::kDefaultDensity if the
// parsing failed.
double ParseIconDensity(const base::DictionaryValue& icon) {
  double density;
  if (!icon.GetDouble("density", &density) || density <= 0)
    return Manifest::Icon::kDefaultDensity;
  return density;
}

std::vector<Manifest::Icon> ParseIcons(const base::DictionaryValue& dictionary,
                                       const GURL& manifest_url) {
  std::vector<Manifest::Icon> icons;
  if (!dictionary.HasKey("icons"))
    return icons;

  const base::ListValue* icons_list = 0;
  if (!dictionary.GetList("icons", &icons_list)) {
    // TODO(mlamouri): provide a custom message to the developer console about
    // the property being incorrectly set.
    return icons;
  }

  for (size_t i = 0; i < icons_list->GetSize(); ++i) {
    const base::DictionaryValue* icon_dictionary = 0;
    if (!icons_list->GetDictionary(i, &icon_dictionary))
      continue;

    Manifest::Icon icon;
    icon.src = ParseIconSrc(*icon_dictionary, manifest_url);
    // An icon MUST have a valid src. If it does not, it MUST be ignored.
    if (!icon.src.is_valid())
      continue;
    icon.type = ParseIconType(*icon_dictionary);
    icon.density = ParseIconDensity(*icon_dictionary);
    // TODO(mlamouri): icon.sizes

    icons.push_back(icon);
  }

  return icons;
}

} // anonymous namespace

Manifest ManifestParser::Parse(const base::StringPiece& json,
                               const GURL& manifest_url,
                               const GURL& document_url) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  if (!value) {
    // TODO(mlamouri): get the JSON parsing error and report it to the developer
    // console.
    return Manifest();
  }

  if (value->GetType() != base::Value::TYPE_DICTIONARY) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return Manifest();
  }

  base::DictionaryValue* dictionary = 0;
  value->GetAsDictionary(&dictionary);
  if (!dictionary) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return Manifest();
  }

  Manifest manifest;

  manifest.name = ParseName(*dictionary);
  manifest.short_name = ParseShortName(*dictionary);
  manifest.start_url = ParseStartURL(*dictionary, manifest_url, document_url);
  manifest.display = ParseDisplay(*dictionary);
  manifest.orientation = ParseOrientation(*dictionary);
  manifest.icons = ParseIcons(*dictionary, manifest_url);

  return manifest;
}

} // namespace content
