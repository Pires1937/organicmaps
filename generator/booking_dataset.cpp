#include "generator/booking_dataset.hpp"
#include "generator/feature_builder.hpp"
#include "generator/sponsored_scoring.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "boost/algorithm/string/replace.hpp"


namespace generator
{
using namespace feature;

// BookingHotel ------------------------------------------------------------------------------------
BookingHotel::BookingHotel(std::string const & src)
{
  std::vector<std::string> rec;
  strings::ParseCSVRow(src, '\t', rec);
  CHECK_EQUAL(rec.size(), FieldsCount(), ("Error parsing hotels.tsv line:",
                                          boost::replace_all_copy(src, "\t", "\\t")));

  CLOG(LDEBUG, strings::to_uint(rec[FieldIndex(Fields::Id)], m_id.Get()), ());
  // TODO(mgsergio): Use ms::LatLon.
  CLOG(LDEBUG, strings::to_double(rec[FieldIndex(Fields::Latitude)], m_latLon.m_lat), ());
  CLOG(LDEBUG, strings::to_double(rec[FieldIndex(Fields::Longtitude)], m_latLon.m_lon), ());

  m_name = rec[FieldIndex(Fields::Name)];
  m_address = rec[FieldIndex(Fields::Address)];

  CLOG(LDEBUG, strings::to_uint(rec[FieldIndex(Fields::Stars)], m_stars), ());
  CLOG(LDEBUG, strings::to_uint(rec[FieldIndex(Fields::PriceCategory)], m_priceCategory), ());
  CLOG(LDEBUG, strings::to_double(rec[FieldIndex(Fields::RatingBooking)], m_ratingBooking), ());
  CLOG(LDEBUG, strings::to_double(rec[FieldIndex(Fields::RatingUsers)], m_ratingUser), ());

  m_descUrl = rec[FieldIndex(Fields::DescUrl)];

  CLOG(LDEBUG, strings::to_uint(rec[FieldIndex(Fields::Type)], m_type), ());

  m_translations = rec[FieldIndex(Fields::Translations)];
}


// BookingDataset ----------------------------------------------------------------------------------
template <>
bool BookingDataset::NecessaryMatchingConditionHolds(FeatureBuilder const & fb) const
{
  if (fb.GetName(StringUtf8Multilang::kDefaultCode).empty())
    return false;

  return ftypes::IsHotelChecker::Instance()(fb.GetTypes());
}

template <>
void BookingDataset::PreprocessMatchedOsmObject(ObjectId, FeatureBuilder & fb, FBuilderFnT const fn) const
{
  // Turn a hotel into a simple building.
  if (fb.GetGeomType() == GeomType::Area)
  {
    // Remove all information about the hotel.
    auto & meta = fb.GetMetadata();
    meta.Drop(Metadata::EType::FMD_STARS);
    meta.Drop(Metadata::EType::FMD_WEBSITE);
    meta.Drop(Metadata::EType::FMD_PHONE_NUMBER);

    auto & params = fb.GetParams();
    params.ClearName();

    auto const tourism = classif().GetTypeByPath({"tourism"});
    base::EraseIf(params.m_types, [tourism](uint32_t type)
    {
      ftype::TruncValue(type, 1);
      return type == tourism;
    });
  }

  fn(fb);
}

template <>
void BookingDataset::BuildObject(Object const & hotel, FBuilderFnT const & fn) const
{
  FeatureBuilder fb;

  fb.SetCenter(mercator::FromLatLon(hotel.m_latLon.m_lat, hotel.m_latLon.m_lon));

  /// @todo SRC_BOOKING
  fb.SetHotelInfo(Metadata::SRC_KAYAK, hotel.m_id.Get(), hotel.m_ratingUser, hotel.m_priceCategory);
  auto & metadata = fb.GetMetadata();
  metadata.Set(Metadata::FMD_WEBSITE, hotel.m_descUrl);
  metadata.Set(Metadata::FMD_STARS, strings::to_string(hotel.m_stars));

  auto & params = fb.GetParams();
  if (!hotel.m_street.empty())
    params.SetStreet(hotel.m_street);

  if (!hotel.m_houseNumber.empty())
    params.AddHouseNumber(hotel.m_houseNumber);

  if (!hotel.m_translations.empty())
  {
    // TODO(mgsergio): Move parsing to the hotel costruction stage.
    std::vector<std::string> parts;
    strings::ParseCSVRow(hotel.m_translations, '|', parts);
    CHECK_EQUAL(parts.size() % 3, 0, ("Invalid translation string:", hotel.m_translations));
    for (size_t i = 0; i < parts.size(); i += 3)
    {
      auto const langCode = StringUtf8Multilang::GetLangIndex(parts[i]);
      params.AddName(StringUtf8Multilang::GetLangByCode(langCode), parts[i + 1]);
      // TODO(mgsergio): e.AddTag("addr:full:" + parts[i], parts[i + 2]);
    }
  }
  params.AddName(StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::kEnglishCode), hotel.m_name);

  auto const & clf = classif();
  params.AddType(clf.GetTypeByPath({"sponsored", "booking"}));
  // Matching booking.com hotel types to OpenStreetMap values.
  // Booking types are listed in the closed API docs.
  switch (hotel.m_type)
  {
    case 19:
    case 205: params.AddType(clf.GetTypeByPath({"tourism", "motel"})); break;

    case 21:
    case 206:
    case 212: params.AddType(clf.GetTypeByPath({"tourism", "resort"})); break;

    case 3:
    case 23:
    case 24:
    case 25:
    case 202:
    case 207:
    case 208:
    case 209:
    case 210:
    case 216:
    case 220:
    case 223: params.AddType(clf.GetTypeByPath({"tourism", "guest_house"})); break;

    case 14:
    case 204:
    case 213:
    case 218:
    case 219:
    case 226:
    case 222: params.AddType(clf.GetTypeByPath({"tourism", "hotel"})); break;

    case 211:
    case 224:
    case 228: params.AddType(clf.GetTypeByPath({"tourism", "chalet"})); break;

    case 13:
    case 225:
    case 203: params.AddType(clf.GetTypeByPath({"tourism", "hostel"})); break;

    case 215:
    case 221:
    case 227:
    case 2:
    case 201: params.AddType(clf.GetTypeByPath({"tourism", "apartment"})); break;

    case 214: params.AddType(clf.GetTypeByPath({"tourism", "camp_site"})); break;

    default: params.AddType(clf.GetTypeByPath({"tourism", "hotel"})); break;
  }

  fn(fb);
}

/// @todo It looks like quite common FindMatchingObjectId function implementation.
template <>
BookingDataset::ObjectId BookingDataset::FindMatchingObjectIdImpl(FeatureBuilder const & fb) const
{
  auto const name = fb.GetName(StringUtf8Multilang::kDefaultCode);

  if (name.empty())
    return Object::InvalidObjectId();

  // Find |kMaxSelectedElements| nearest values to a point, sorted by distance?
  auto const bookingIndexes = m_storage.GetNearestObjects(mercator::ToLatLon(fb.GetKeyPoint()));

  /// @todo Select best candidate? Assume we match "Foo Resort SPA hotel" feature. Have candidates:
  /// - "Bar SPA hotel" in 10 meters (first);
  /// - "Foo SPA hotel" in 100 meters (second, but best);
  /// I suspect that first "Bar hotel" will be selected (wrong).
  for (auto const j : bookingIndexes)
  {
    if (sponsored_scoring::Match(m_storage.GetObjectById(j), fb).IsMatched())
      return j;
  }

  return Object::InvalidObjectId();
}
}  // namespace generator
