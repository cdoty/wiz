#ifndef WIZ_FORMAT_SMS_FORMAT_H
#define WIZ_FORMAT_SMS_FORMAT_H

#include <wiz/format/format.h>
#include <wiz/utility/fwd_unique_ptr.h>

namespace wiz {
    class SmsFormat : public Format {
        public:
            enum class SystemType {
                MasterSystem,
                GameGear
            };

            SmsFormat(SystemType systemType);
            ~SmsFormat() override;

            bool generate(FormatContext& context) override;
        private:
            SystemType systemType;
    };
}

#endif