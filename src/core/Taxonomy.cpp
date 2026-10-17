// SPDX-License-Identifier: BSD-2-Clause
#include "core/Taxonomy.h"

#include <QCoreApplication>
#include <QHash>

namespace pix {

namespace {

// O nome vai com QT_TRANSLATE_NOOP em cada linha (o lupdate não expande macros).
const QList<BuiltinCategory> kCategories = {
    // 1. Pessoas
    BuiltinCategory{"people", nullptr, QT_TRANSLATE_NOOP("Category", "People"), ""},
    BuiltinCategory{"people.portrait", "people", QT_TRANSLATE_NOOP("Category", "Portrait"), "a portrait photo of a person|a headshot of a person"},
    BuiltinCategory{"people.selfie", "people", QT_TRANSLATE_NOOP("Category", "Selfie"), "a selfie taken with a phone|a person taking a selfie"},
    BuiltinCategory{"people.group", "people", QT_TRANSLATE_NOOP("Category", "Group"), "a group photo of many people|a large group of people posing together"},
    BuiltinCategory{"people.family", "people", QT_TRANSLATE_NOOP("Category", "Family"), "a family photo with parents and children"},
    BuiltinCategory{"people.child", "people", QT_TRANSLATE_NOOP("Category", "Baby / Child"), "a photo of a baby|a photo of a young child"},
    BuiltinCategory{"people.couple", "people", QT_TRANSLATE_NOOP("Category", "Couple"), "a romantic couple posing together"},
    BuiltinCategory{"people.elderly", "people", QT_TRANSLATE_NOOP("Category", "Elderly"), "a photo of an elderly person"},
    BuiltinCategory{"people.fashion", "people", QT_TRANSLATE_NOOP("Category", "Fashion / Outfit"), "a fashion photo of a model showing an outfit"},
    BuiltinCategory{"people.fitness", "people", QT_TRANSLATE_NOOP("Category", "Body / Fitness"), "a fitness photo of a muscular body"},

    // 2. Paisagem & Natureza
    BuiltinCategory{"nature", nullptr, QT_TRANSLATE_NOOP("Category", "Landscape & Nature"), ""},
    BuiltinCategory{"nature.beach", "nature", QT_TRANSLATE_NOOP("Category", "Beach / Sea"), "a photo of a beach|a photo of the sea and waves"},
    BuiltinCategory{"nature.mountain", "nature", QT_TRANSLATE_NOOP("Category", "Mountain"), "a photo of mountains"},
    BuiltinCategory{"nature.forest", "nature", QT_TRANSLATE_NOOP("Category", "Forest / Woods"), "a photo of a forest|a photo of woods and trees"},
    BuiltinCategory{"nature.countryside", "nature", QT_TRANSLATE_NOOP("Category", "Countryside / Rural"), "a photo of the countryside with farm fields"},
    BuiltinCategory{"nature.desert", "nature", QT_TRANSLATE_NOOP("Category", "Desert"), "a photo of a desert with sand dunes"},
    BuiltinCategory{"nature.water", "nature", QT_TRANSLATE_NOOP("Category", "River / Lake / Waterfall"), "a photo of a river|a photo of a lake|a photo of a waterfall"},
    BuiltinCategory{"nature.snow", "nature", QT_TRANSLATE_NOOP("Category", "Snow / Ice"), "a snowy landscape|a photo of snow and ice"},
    BuiltinCategory{"nature.sky", "nature", QT_TRANSLATE_NOOP("Category", "Sky / Clouds"), "a photo of the sky with clouds"},
    BuiltinCategory{"nature.sunset", "nature", QT_TRANSLATE_NOOP("Category", "Sunset / Sunrise"), "a photo of a sunset|a photo of a sunrise"},
    BuiltinCategory{"nature.night", "nature", QT_TRANSLATE_NOOP("Category", "Night / Stars / Astrophotography"), "a night sky full of stars|an astrophotography photo of the milky way"},
    BuiltinCategory{"nature.flowers", "nature", QT_TRANSLATE_NOOP("Category", "Flowers / Plants"), "a close-up photo of flowers|a photo of plants"},
    BuiltinCategory{"nature.weather", "nature", QT_TRANSLATE_NOOP("Category", "Weather (rain, storm, fog)"), "a photo of rain|a photo of a storm with lightning|a foggy landscape"},

    // 3. Cidade & Arquitetura
    BuiltinCategory{"city", nullptr, QT_TRANSLATE_NOOP("Category", "City & Architecture"), ""},
    BuiltinCategory{"city.skyline", "city", QT_TRANSLATE_NOOP("Category", "Cityscape / Skyline"), "a city skyline|an aerial view of a city"},
    BuiltinCategory{"city.street", "city", QT_TRANSLATE_NOOP("Category", "Street"), "a photo of a city street"},
    BuiltinCategory{"city.building", "city", QT_TRANSLATE_NOOP("Category", "Building"), "a photo of a building facade"},
    BuiltinCategory{"city.landmark", "city", QT_TRANSLATE_NOOP("Category", "Monument / Landmark"), "a photo of a famous monument"},
    BuiltinCategory{"city.church", "city", QT_TRANSLATE_NOOP("Category", "Church / Temple"), "a photo of a church|a photo of a temple"},
    BuiltinCategory{"city.bridge", "city", QT_TRANSLATE_NOOP("Category", "Bridge"), "a photo of a bridge"},
    BuiltinCategory{"city.interior", "city", QT_TRANSLATE_NOOP("Category", "Interior (house, living room, bedroom, kitchen)"), "a photo of a living room|a photo of a bedroom|a photo of a kitchen"},
    BuiltinCategory{"city.construction", "city", QT_TRANSLATE_NOOP("Category", "Construction site"), "a photo of a construction site"},

    // 4. Festa & Evento
    BuiltinCategory{"event", nullptr, QT_TRANSLATE_NOOP("Category", "Party & Event"), ""},
    BuiltinCategory{"event.birthday", "event", QT_TRANSLATE_NOOP("Category", "Birthday"), "a birthday party with a cake and candles"},
    BuiltinCategory{"event.wedding", "event", QT_TRANSLATE_NOOP("Category", "Wedding"), "a wedding ceremony with a bride and groom"},
    BuiltinCategory{"event.engagement", "event", QT_TRANSLATE_NOOP("Category", "Engagement"), "an engagement photo with a ring"},
    BuiltinCategory{"event.babyshower", "event", QT_TRANSLATE_NOOP("Category", "Baby shower / Gender reveal"), "a baby shower party|a gender reveal party"},
    BuiltinCategory{"event.baptism", "event", QT_TRANSLATE_NOOP("Category", "Baptism"), "a baby being baptized in a church|a baptism ceremony at a church font"},
    BuiltinCategory{"event.graduation", "event", QT_TRANSLATE_NOOP("Category", "Graduation"), "a graduation ceremony with caps and gowns|students receiving diplomas at a graduation ceremony|children at a school graduation ceremony"},
    BuiltinCategory{"event.debutante", "event", QT_TRANSLATE_NOOP("Category", "Sweet fifteen / Debutante"), "a quinceanera party with a girl in a ball gown|a debutante ball with a girl in a long gown"},
    BuiltinCategory{"event.barbecue", "event", QT_TRANSLATE_NOOP("Category", "Barbecue party"), "people at a barbecue party"},
    BuiltinCategory{"event.junefestival", "event", QT_TRANSLATE_NOOP("Category", "June festival"), "a brazilian festa junina with colorful flags and a bonfire|people in country costumes at a june festival"},
    BuiltinCategory{"event.christmas", "event", QT_TRANSLATE_NOOP("Category", "Christmas"), "a christmas tree with decorations|a christmas celebration with santa claus"},
    BuiltinCategory{"event.newyear", "event", QT_TRANSLATE_NOOP("Category", "New Year's Eve"), "new year's eve fireworks celebration"},
    BuiltinCategory{"event.carnival", "event", QT_TRANSLATE_NOOP("Category", "Carnival"), "a carnival parade with costumes"},
    BuiltinCategory{"event.easter", "event", QT_TRANSLATE_NOOP("Category", "Easter"), "easter eggs and an easter celebration"},
    BuiltinCategory{"event.halloween", "event", QT_TRANSLATE_NOOP("Category", "Halloween"), "a halloween party with costumes and pumpkins"},
    BuiltinCategory{"event.nightclub", "event", QT_TRANSLATE_NOOP("Category", "Nightclub / Night party"), "people dancing at a nightclub"},
    BuiltinCategory{"event.concert", "event", QT_TRANSLATE_NOOP("Category", "Concert / Music festival"), "a live music concert on stage|a crowd at a music festival"},
    BuiltinCategory{"event.corporate", "event", QT_TRANSLATE_NOOP("Category", "Corporate gathering"), "a corporate event or company party"},
    BuiltinCategory{"event.conference", "event", QT_TRANSLATE_NOOP("Category", "Lecture / Conference"), "a speaker giving a talk at a conference"},
    BuiltinCategory{"event.religious", "event", QT_TRANSLATE_NOOP("Category", "Religious service"), "a religious service in a church"},
    BuiltinCategory{"event.funeral", "event", QT_TRANSLATE_NOOP("Category", "Funeral / Memorial"), "a funeral or memorial service"},

    // 5. Esportes
    BuiltinCategory{"sports", nullptr, QT_TRANSLATE_NOOP("Category", "Sports"), ""},
    BuiltinCategory{"sports.soccer", "sports", QT_TRANSLATE_NOOP("Category", "Soccer (field, society, futsal)"), "people playing soccer on a grass field|a football match with a goal and a net|a player kicking a soccer ball|a futsal match with small goals"},
    BuiltinCategory{"sports.basketball", "sports", QT_TRANSLATE_NOOP("Category", "Basketball"), "people playing basketball|a basketball game on an indoor court|a player dribbling a basketball|a basketball player shooting at the hoop"},
    BuiltinCategory{"sports.volleyball", "sports", QT_TRANSLATE_NOOP("Category", "Volleyball (court, beach)"), "people playing volleyball over a net|a volleyball player spiking the ball over the net|a beach volleyball game on sand"},
    BuiltinCategory{"sports.tennis", "sports", QT_TRANSLATE_NOOP("Category", "Tennis / Beach tennis / Padel"), "people playing tennis with rackets|a tennis player hitting a ball with a racket|a padel match with rackets"},
    BuiltinCategory{"sports.cycling", "sports", QT_TRANSLATE_NOOP("Category", "Cycling / Mountain bike"), "a cyclist riding a bicycle in a race|mountain biking on a trail"},
    BuiltinCategory{"sports.running", "sports", QT_TRANSLATE_NOOP("Category", "Running / Athletics"), "runners in a road race wearing race numbers|an athletics competition on a running track"},
    BuiltinCategory{"sports.swimming", "sports", QT_TRANSLATE_NOOP("Category", "Swimming"), "a swimmer in a swimming pool|people swimming in the water"},
    BuiltinCategory{"sports.surf", "sports", QT_TRANSLATE_NOOP("Category", "Surfing / Bodyboarding"), "a surfer riding a wave"},
    BuiltinCategory{"sports.skate", "sports", QT_TRANSLATE_NOOP("Category", "Skateboarding"), "a skateboarder doing a trick on a skateboard|a person riding a skateboard"},
    BuiltinCategory{"sports.martialarts", "sports", QT_TRANSLATE_NOOP("Category", "Martial arts (jiu-jitsu, judo, boxing, MMA)"), "a martial arts fight|a boxing match|jiu-jitsu fighters"},
    BuiltinCategory{"sports.gym", "sports", QT_TRANSLATE_NOOP("Category", "Gym / Weightlifting / Crossfit"), "a person lifting weights at the gym"},
    BuiltinCategory{"sports.yoga", "sports", QT_TRANSLATE_NOOP("Category", "Yoga / Pilates"), "a person doing yoga"},
    BuiltinCategory{"sports.gymnastics", "sports", QT_TRANSLATE_NOOP("Category", "Gymnastics"), "a gymnast performing on gymnastics apparatus|a gymnast on a balance beam"},
    BuiltinCategory{"sports.motorsport", "sports", QT_TRANSLATE_NOOP("Category", "Motorsport"), "a car race on a track|a motorcycle race"},
    BuiltinCategory{"sports.winter", "sports", QT_TRANSLATE_NOOP("Category", "Winter sports (skiing, snowboarding)"), "a person skiing|a snowboarder on a snowy slope"},
    BuiltinCategory{"sports.water", "sports", QT_TRANSLATE_NOOP("Category", "Water sports (sailing, kayaking, stand-up paddle)"), "a sailboat regatta|a person kayaking|stand up paddle boarding"},
    BuiltinCategory{"sports.climbing", "sports", QT_TRANSLATE_NOOP("Category", "Climbing / Hiking / Mountaineering"), "a rock climber on a cliff|hikers on a mountain trail"},
    BuiltinCategory{"sports.golf", "sports", QT_TRANSLATE_NOOP("Category", "Golf"), "a person playing golf"},
    BuiltinCategory{"sports.equestrian", "sports", QT_TRANSLATE_NOOP("Category", "Equestrian"), "a rider on a horse at an equestrian event"},
    BuiltinCategory{"sports.esports", "sports", QT_TRANSLATE_NOOP("Category", "eSports"), "gamers playing video games at an esports tournament|people playing video games on computers"},

    // 6. Veículos & Transporte
    BuiltinCategory{"vehicles", nullptr, QT_TRANSLATE_NOOP("Category", "Vehicles & Transport"), ""},
    BuiltinCategory{"vehicles.car", "vehicles", QT_TRANSLATE_NOOP("Category", "Car"), "a photo of a car"},
    BuiltinCategory{"vehicles.motorcycle", "vehicles", QT_TRANSLATE_NOOP("Category", "Motorcycle"), "a photo of a motorcycle"},
    BuiltinCategory{"vehicles.truck", "vehicles", QT_TRANSLATE_NOOP("Category", "Truck / Bus"), "a photo of a truck|a photo of a bus"},
    BuiltinCategory{"vehicles.bicycle", "vehicles", QT_TRANSLATE_NOOP("Category", "Bicycle"), "a photo of a bicycle"},
    BuiltinCategory{"vehicles.aircraft", "vehicles", QT_TRANSLATE_NOOP("Category", "Airplane / Helicopter"), "a photo of an airplane|a photo of a helicopter"},
    BuiltinCategory{"vehicles.boat", "vehicles", QT_TRANSLATE_NOOP("Category", "Boat / Ship"), "a boat on the water|a ship at sea"},
    BuiltinCategory{"vehicles.train", "vehicles", QT_TRANSLATE_NOOP("Category", "Train / Subway"), "a photo of a train|a subway train in a station"},
    BuiltinCategory{"vehicles.classic", "vehicles", QT_TRANSLATE_NOOP("Category", "Vintage / Classic vehicle"), "a vintage classic car"},

    // 7. Animais
    BuiltinCategory{"animals", nullptr, QT_TRANSLATE_NOOP("Category", "Animals"), ""},
    BuiltinCategory{"animals.dog", "animals", QT_TRANSLATE_NOOP("Category", "Dog"), "a photo of a dog"},
    BuiltinCategory{"animals.cat", "animals", QT_TRANSLATE_NOOP("Category", "Cat"), "a photo of a cat"},
    BuiltinCategory{"animals.bird", "animals", QT_TRANSLATE_NOOP("Category", "Bird"), "a photo of a bird"},
    BuiltinCategory{"animals.fish", "animals", QT_TRANSLATE_NOOP("Category", "Fish / Marine life"), "a photo of fish|underwater marine life"},
    BuiltinCategory{"animals.farm", "animals", QT_TRANSLATE_NOOP("Category", "Farm animal"), "a photo of farm animals like cows, horses or chickens"},
    BuiltinCategory{"animals.wild", "animals", QT_TRANSLATE_NOOP("Category", "Wild animal"), "a photo of a wild animal in nature"},
    BuiltinCategory{"animals.insect", "animals", QT_TRANSLATE_NOOP("Category", "Insect"), "a close-up photo of an insect|a butterfly|a bee on a flower"},
    BuiltinCategory{"animals.reptile", "animals", QT_TRANSLATE_NOOP("Category", "Reptile / Amphibian"), "a photo of a reptile|a photo of a frog"},

    // 8. Comida & Bebida
    BuiltinCategory{"food", nullptr, QT_TRANSLATE_NOOP("Category", "Food & Drink"), ""},
    BuiltinCategory{"food.meal", "food", QT_TRANSLATE_NOOP("Category", "Dish / Meal"), "a plate of food|a meal served on a plate"},
    BuiltinCategory{"food.fastfood", "food", QT_TRANSLATE_NOOP("Category", "Snack / Fast food"), "a hamburger and fries|fast food"},
    BuiltinCategory{"food.dessert", "food", QT_TRANSLATE_NOOP("Category", "Dessert / Sweets / Cake"), "a dessert|a cake|sweets and candy"},
    BuiltinCategory{"food.fruit", "food", QT_TRANSLATE_NOOP("Category", "Fruit / Vegetables"), "fresh fruit|fresh vegetables"},
    BuiltinCategory{"food.meat", "food", QT_TRANSLATE_NOOP("Category", "Barbecue / Meat"), "grilled meat on a barbecue"},
    BuiltinCategory{"food.drink", "food", QT_TRANSLATE_NOOP("Category", "Drink (coffee, juice)"), "a cup of coffee|a glass of juice"},
    BuiltinCategory{"food.alcohol", "food", QT_TRANSLATE_NOOP("Category", "Alcoholic drink (beer, wine, cocktails)"), "a glass of beer|a glass of wine|a cocktail"},
    BuiltinCategory{"food.restaurant", "food", QT_TRANSLATE_NOOP("Category", "Restaurant / Table setting"), "a restaurant interior|a set dinner table"},

    // 9. Viagem & Turismo
    BuiltinCategory{"travel", nullptr, QT_TRANSLATE_NOOP("Category", "Travel & Tourism"), ""},
    BuiltinCategory{"travel.hotel", "travel", QT_TRANSLATE_NOOP("Category", "Hotel / Lodging"), "a hotel room|a hotel lobby"},
    BuiltinCategory{"travel.airport", "travel", QT_TRANSLATE_NOOP("Category", "Airport"), "a photo inside an airport terminal"},
    BuiltinCategory{"travel.attraction", "travel", QT_TRANSLATE_NOOP("Category", "Tourist attraction"), "tourists at a famous tourist attraction"},
    BuiltinCategory{"travel.cruise", "travel", QT_TRANSLATE_NOOP("Category", "Cruise"), "a cruise ship|the deck of a cruise ship"},
    BuiltinCategory{"travel.camping", "travel", QT_TRANSLATE_NOOP("Category", "Camping"), "a tent at a campsite"},

    // 10. Trabalho & Educação
    BuiltinCategory{"work", nullptr, QT_TRANSLATE_NOOP("Category", "Work & Education"), ""},
    BuiltinCategory{"work.office", "work", QT_TRANSLATE_NOOP("Category", "Office"), "an office with desks and computers"},
    BuiltinCategory{"work.meeting", "work", QT_TRANSLATE_NOOP("Category", "Meeting"), "people in a business meeting"},
    BuiltinCategory{"work.school", "work", QT_TRANSLATE_NOOP("Category", "School / Classroom"), "students in a school classroom|a teacher in front of a classroom"},
    BuiltinCategory{"work.university", "work", QT_TRANSLATE_NOOP("Category", "University"), "a university campus|a university lecture hall"},
    BuiltinCategory{"work.profession", "work", QT_TRANSLATE_NOOP("Category", "Profession (doctor, chef, worker...)"), "a doctor at work|a chef cooking in a kitchen|a worker at work"},
    BuiltinCategory{"work.tools", "work", QT_TRANSLATE_NOOP("Category", "Tools / Equipment"), "tools and equipment"},

    // 11. Objetos & Produtos
    BuiltinCategory{"objects", nullptr, QT_TRANSLATE_NOOP("Category", "Objects & Products"), ""},
    BuiltinCategory{"objects.electronics", "objects", QT_TRANSLATE_NOOP("Category", "Electronics (phone, computer)"), "a smartphone|a laptop computer|electronic devices"},
    BuiltinCategory{"objects.clothing", "objects", QT_TRANSLATE_NOOP("Category", "Clothing / Shoes / Accessories"), "clothes|a pair of shoes|fashion accessories"},
    BuiltinCategory{"objects.furniture", "objects", QT_TRANSLATE_NOOP("Category", "Furniture / Decor"), "furniture|home decoration"},
    BuiltinCategory{"objects.jewelry", "objects", QT_TRANSLATE_NOOP("Category", "Jewelry / Watch"), "jewelry|a wristwatch"},
    BuiltinCategory{"objects.toy", "objects", QT_TRANSLATE_NOOP("Category", "Toy"), "a toy"},
    BuiltinCategory{"objects.book", "objects", QT_TRANSLATE_NOOP("Category", "Book"), "a book|books on a shelf"},
    BuiltinCategory{"objects.product", "objects", QT_TRANSLATE_NOOP("Category", "Product (e-commerce / catalog)"), "a product photo on a white background|a packshot of a product for an online store"},
    BuiltinCategory{"objects.instrument", "objects", QT_TRANSLATE_NOOP("Category", "Musical instrument"), "a musical instrument such as a guitar or piano"},

    // 12. Arte & Cultura
    BuiltinCategory{"art", nullptr, QT_TRANSLATE_NOOP("Category", "Art & Culture"), ""},
    BuiltinCategory{"art.museum", "art", QT_TRANSLATE_NOOP("Category", "Museum / Exhibition"), "an art museum exhibition"},
    BuiltinCategory{"art.theater", "art", QT_TRANSLATE_NOOP("Category", "Theater / Dance"), "a theater play on a stage with lights|a dance performance on stage"},
    BuiltinCategory{"art.graffiti", "art", QT_TRANSLATE_NOOP("Category", "Graffiti / Street art"), "graffiti street art on a wall"},
    BuiltinCategory{"art.sculpture", "art", QT_TRANSLATE_NOOP("Category", "Sculpture"), "a sculpture|a statue"},
    BuiltinCategory{"art.cinema", "art", QT_TRANSLATE_NOOP("Category", "Cinema / Fine-art photography"), "a movie theater|a fine art photograph"},

    // 13. Saúde & Bem-estar
    BuiltinCategory{"health", nullptr, QT_TRANSLATE_NOOP("Category", "Health & Wellness"), ""},
    BuiltinCategory{"health.hospital", "health", QT_TRANSLATE_NOOP("Category", "Hospital / Clinic"), "a hospital room|a medical clinic"},
    BuiltinCategory{"health.exam", "health", QT_TRANSLATE_NOOP("Category", "Medical exam / X-ray"), "an x-ray image|a medical scan|an mri scan of a body"},
    BuiltinCategory{"health.beauty", "health", QT_TRANSLATE_NOOP("Category", "Spa / Beauty (hair, makeup, nails)"), "a beauty salon|makeup being applied to a face|manicured nails at a nail salon|a hairdresser cutting hair"},

    // 14. Tecnologia & Ciência
    BuiltinCategory{"tech", nullptr, QT_TRANSLATE_NOOP("Category", "Technology & Science"), ""},
    BuiltinCategory{"tech.code", "tech", QT_TRANSLATE_NOOP("Category", "Code / Programming"), "a screenshot of programming code in a code editor|source code on a dark screen"},
    BuiltinCategory{"tech.hardware", "tech", QT_TRANSLATE_NOOP("Category", "Hardware / Circuits"), "an electronic circuit board"},
    BuiltinCategory{"tech.lab", "tech", QT_TRANSLATE_NOOP("Category", "Laboratory"), "a science laboratory"},
    BuiltinCategory{"tech.space", "tech", QT_TRANSLATE_NOOP("Category", "Space / Astronomy"), "a telescope image of planets and galaxies"},

    // 15. Abstrato & Texturas
    BuiltinCategory{"abstract", nullptr, QT_TRANSLATE_NOOP("Category", "Abstract & Textures"), ""},
    BuiltinCategory{"abstract.pattern", "abstract", QT_TRANSLATE_NOOP("Category", "Pattern"), "a repeating abstract pattern"},
    BuiltinCategory{"abstract.texture", "abstract", QT_TRANSLATE_NOOP("Category", "Texture (wood, stone, fabric)"), "a close-up texture of wood|a stone texture|a fabric texture"},
    BuiltinCategory{"abstract.gradient", "abstract", QT_TRANSLATE_NOOP("Category", "Gradient / Colors"), "an abstract color gradient"},
    BuiltinCategory{"abstract.minimalist", "abstract", QT_TRANSLATE_NOOP("Category", "Minimalist"), "a minimalist photo with a lot of empty space"},

    // 16. Outros
    BuiltinCategory{"other", nullptr, QT_TRANSLATE_NOOP("Category", "Other"), ""},
    BuiltinCategory{"other.uncategorized", "other", QT_TRANSLATE_NOOP("Category", "Uncategorized"), ""},
    BuiltinCategory{"other.blurry", "other", QT_TRANSLATE_NOOP("Category", "Blurry / Low quality"), ""},
    BuiltinCategory{"other.duplicate", "other", QT_TRANSLATE_NOOP("Category", "Duplicate"), ""},
    BuiltinCategory{"other.sensitive", "other", QT_TRANSLATE_NOOP("Category", "Sensitive content (NSFW / violence)"), "an explicit nude photo|a violent bloody scene"},
    // Logotipos, ícones e figuras de site/programa (comuns em pastas de
    // trabalho): sem ela o CLIP os chamava de "exame médico" ou "inseto".
    BuiltinCategory{"other.graphic", "other", QT_TRANSLATE_NOOP("Category", "Logo / Icon / Graphic"), "a logo|an icon|a simple graphic on a white background|a clip art illustration"},
    // Documentos e capturas de tela não estão na lista sugerida, mas sem eles
    // o CLIP empurra prints e notas fiscais para "Código" ou "Produto".
    BuiltinCategory{"other.screenshot", "other", QT_TRANSLATE_NOOP("Category", "Screenshot / Document"), "a screenshot of a phone or computer screen|a screenshot of a software window|a screenshot of a website|a photo of a document or receipt"},
};

} // namespace

const QList<BuiltinCategory> &builtinCategories()
{
    return kCategories;
}

QString builtinCategoryName(const QString &key)
{
    for (const BuiltinCategory &c : kCategories)
        if (key == QLatin1String(c.key))
            return QCoreApplication::translate("Category", c.name);
    return {};
}

QStringList clipPrompts(const QString &prompt, const QString &name)
{
    QStringList out;
    for (const QString &p : prompt.split(u'|', Qt::SkipEmptyParts))
        if (!p.trimmed().isEmpty())
            out << p.trimmed();
    if (out.isEmpty() && !name.trimmed().isEmpty())
        out << QStringLiteral("a photo of %1").arg(name.trimmed().toLower());
    return out;
}

const QStringList &backgroundPrompts()
{
    // Genéricas de propósito: só vencem quando nenhuma categoria descreve a
    // foto melhor que "uma imagem qualquer".
    static const QStringList prompts{QStringLiteral("a photo"), QStringLiteral("an image"), QStringLiteral("a picture of something"),
                                     QStringLiteral("a blank image"), QStringLiteral("a white background")};
    return prompts;
}

QStringList previousPrompts(const QString &key)
{
    // As frases que cada embutida já teve (versões anteriores do app). Se o
    // banco ainda guarda uma delas, a pessoa não a mudou: troca pela atual.
    static const QHash<QString, QStringList> previous{
        {QStringLiteral("sports.basketball"), {QStringLiteral("people playing basketball")}},
        {QStringLiteral("sports.soccer"), {QStringLiteral("people playing soccer|a football match")}},
        {QStringLiteral("sports.volleyball"), {QStringLiteral("people playing volleyball|a beach volleyball game")}},
        {QStringLiteral("sports.tennis"), {QStringLiteral("people playing tennis|a padel match")}},
        {QStringLiteral("sports.running"), {QStringLiteral("runners in a race|an athletics competition on a track")}},
        {QStringLiteral("sports.swimming"), {QStringLiteral("a swimmer in a swimming pool")}},
        {QStringLiteral("sports.skate"), {QStringLiteral("a skateboarder doing a trick")}},
        {QStringLiteral("sports.gymnastics"), {QStringLiteral("a gymnast performing")}},
        {QStringLiteral("sports.esports"), {QStringLiteral("gamers playing video games at an esports tournament")}},
        {QStringLiteral("event.graduation"), {QStringLiteral("a graduation ceremony with caps and gowns")}},
        {QStringLiteral("event.baptism"), {QStringLiteral("a baptism ceremony in a church")}},
        {QStringLiteral("event.debutante"), {QStringLiteral("a quinceanera party with a girl in a ball gown")}},
        {QStringLiteral("event.christmas"), {QStringLiteral("a christmas celebration with a christmas tree")}},
        {QStringLiteral("event.junefestival"), {QStringLiteral("a brazilian festa junina with colorful flags and bonfire")}},
        {QStringLiteral("health.beauty"), {QStringLiteral("a beauty salon|makeup being applied|manicured nails")}},
        {QStringLiteral("health.exam"), {QStringLiteral("an x-ray image|a medical scan")}},
        {QStringLiteral("art.theater"), {QStringLiteral("a theater play on stage|a dance performance")}},
        {QStringLiteral("vehicles.boat"), {QStringLiteral("a photo of a boat|a photo of a ship")}},
        {QStringLiteral("animals.insect"), {QStringLiteral("a close-up photo of an insect")}},
        {QStringLiteral("objects.product"), {QStringLiteral("a product photo on a white background")}},
        {QStringLiteral("work.school"), {QStringLiteral("a school classroom with students")}},
        {QStringLiteral("other.screenshot"), {QStringLiteral("a screenshot of a phone or computer screen|a photo of a document or receipt")}},
        {QStringLiteral("tech.code"), {QStringLiteral("a screenshot of programming code")}},
    };
    return previous.value(key);
}

} // namespace pix
