#
#  Zigbee 3.0 4-channel LED strip driver.
#  Copyright (C) 2022 Andrzej Gendek
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
 
eval $(sed -n 's/^#define EMBER_AF_*\([^ ]*\)  *\(.*\) *$/export \1=\2/p' app.h)

APP_NAME=LedDrv
OUT_DIR="./Release"

(cd "$OUT_DIR";
/opt/SimplicityStudio_v5/developer/adapter_packs/commander/commander gbl create $APP_NAME.gbl --app $APP_NAME.s37;
~/SimplicityStudio/SDKs/gecko_sdk_2/protocol/zigbee/tool/image-builder/image-builder-linux -t 0 -f $APP_NAME.gbl -c $APP_NAME.ota -v $CUSTOM_FIRMWARE_VERSION -m 0x1002 -i $IMAGE_TYPE_ID)
