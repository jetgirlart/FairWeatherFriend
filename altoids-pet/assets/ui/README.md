# Location-source UI icon slots

Reserved future authoritative PNG names:

- `location_gnss.png` — satellite/dish
- `location_field_site.png` — pin/house
- `location_none.png` — unknown/crossed location

No final PNG artwork is generated or supplied here. Current icons are minimal
7×7 transparent pixel masks in `firmware/altoids_pet/location_icon.h`, rendered
beside HOME and Latest Observation/Field Note labels. They encode the actual
observation source, not module presence. Replace the mask rows to edit these
placeholders now.

The existing converter currently supports 48×48 buddy/gear and 240×240 backgrounds,
not small UI PNGs. These names reserve future 7×7 RGBA semantic-color sources;
adding PNG files alone does not activate them yet. A future dedicated converter
registry can consume them without changing observation/source logic. Do not run
these tiny assets through the buddy registry or scale them into full pet frames.
