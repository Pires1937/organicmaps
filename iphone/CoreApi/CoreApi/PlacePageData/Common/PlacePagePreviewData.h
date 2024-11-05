#import <Foundation/Foundation.h>

@class PlacePageScheduleData;

typedef NS_ENUM(NSInteger, PlacePageDataHotelType) {
  PlacePageDataHotelTypeHotel,
  PlacePageDataHotelTypeApartment,
  PlacePageDataHotelTypeCampSite,
  PlacePageDataHotelTypeChalet,
  PlacePageDataHotelTypeGuestHouse,
  PlacePageDataHotelTypeHostel,
  PlacePageDataHotelTypeMotel,
  PlacePageDataHotelTypeResort,
  PlacePageDataHotelTypeNone
};

typedef NS_ENUM(NSInteger, PlacePageDataOpeningHours) {
  PlacePageDataOpeningHoursAllDay,
  PlacePageDataOpeningHoursOpen,
  PlacePageDataOpeningHoursClosed,
  PlacePageDataOpeningHoursUnknown
};

typedef struct {
    PlacePageDataOpeningHours state;
    time_t nextTimeOpen;
    time_t nextTimeClosed;
} PlacePageDataSchedule;

NS_ASSUME_NONNULL_BEGIN

@interface PlacePagePreviewData : NSObject

@property(nonatomic, readonly, nullable) NSString *title;
@property(nonatomic, readonly, nullable) NSString *secondaryTitle;
@property(nonatomic, readonly, nullable) NSString *subtitle;
@property(nonatomic, readonly, nullable) NSString *coordinates;
@property(nonatomic, readonly, nullable) NSString *secondarySubtitle;
@property(nonatomic, readonly) PlacePageDataSchedule schedule;
@property(nonatomic, readonly) BOOL isMyPosition;

@end

NS_ASSUME_NONNULL_END
