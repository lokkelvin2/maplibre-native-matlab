#include <mbgl/map/map.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/util/image.hpp>
#include <mbgl/util/run_loop.hpp>

#include <mbgl/gfx/backend.hpp>
#include <mbgl/gfx/headless_frontend.hpp>
#include <mbgl/style/style.hpp>

#include <args.hxx>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iomanip>

int main(int argc, char* argv[]) {
    args::ArgumentParser argumentParser("Mapbox GL render tool");
    args::HelpFlag helpFlag(argumentParser, "help", "Display this help menu", {"help"});

    args::ValueFlag<std::string> backendValue(argumentParser, "Backend", "Rendering backend", {"backend"});
    args::ValueFlag<std::string> apikeyValue(argumentParser, "key", "API key", {'t', "apikey"});
    args::ValueFlag<std::string> styleValue(argumentParser, "URL", "Map stylesheet", {'s', "style"});
    args::ValueFlag<std::string> outputValue(argumentParser, "file", "Output file name", {'o', "output"});
    args::ValueFlag<std::string> cacheValue(argumentParser, "file", "Cache database file name", {'c', "cache"});
    args::ValueFlag<std::string> assetsValue(
        argumentParser, "file", "Directory to which asset:// URLs will resolve", {'a', "assets"});

    args::Flag debugFlag(argumentParser, "debug", "Debug mode", {"debug"});

    args::ValueFlag<double> pixelRatioValue(argumentParser, "number", "Image scale factor", {'r', "ratio"});

    args::ValueFlag<double> zoomValue(argumentParser, "number", "Zoom level", {'z', "zoom"});

    args::ValueFlag<double> lonValue(argumentParser, "degrees", "Longitude", {'x', "lon"});
    args::ValueFlag<double> latValue(argumentParser, "degrees", "Latitude", {'y', "lat"});
    args::ValueFlag<double> bearingValue(argumentParser, "degrees", "Bearing", {'b', "bearing"});
    args::ValueFlag<double> pitchValue(argumentParser, "degrees", "Pitch", {'p', "pitch"});
    args::ValueFlag<uint32_t> widthValue(argumentParser, "pixels", "Image width", {'w', "width"});
    args::ValueFlag<uint32_t> heightValue(argumentParser, "pixels", "Image height", {'h', "height"});
    args::ValueFlag<double> minlonValue(argumentParser, "degrees", "Minimum Longitude", { "minlon" });
    args::ValueFlag<double> maxlonValue(argumentParser, "degrees", "Maximum Longitude", { "maxlon" });
    args::ValueFlag<double> minlatValue(argumentParser, "degrees", "Minimum Latitude", { "minlat" });
    args::ValueFlag<double> maxlatValue(argumentParser, "degrees", "Maximum Latitude", { "maxlat" });

    try {
        argumentParser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::cout << argumentParser;
        exit(0);
    } catch (const args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << argumentParser;
        exit(1);
    } catch (const args::ValidationError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << argumentParser;
        exit(2);
    }

    const double lat = latValue ? args::get(latValue) : 0;
    const double lon = lonValue ? args::get(lonValue) : 0;
    const double zoom = zoomValue ? args::get(zoomValue) : 0;
    const double bearing = bearingValue ? args::get(bearingValue) : 0;
    const double pitch = pitchValue ? args::get(pitchValue) : 0;
    const double pixelRatio = pixelRatioValue ? args::get(pixelRatioValue) : 1;

    const uint32_t width = widthValue ? args::get(widthValue) : 512;
    const uint32_t height = heightValue ? args::get(heightValue) : 512;
    const std::string output = outputValue ? args::get(outputValue) : "out.png";
    const std::string cache_file = cacheValue ? args::get(cacheValue) : "cache.sqlite";
    const std::string asset_root = assetsValue ? args::get(assetsValue) : ".";

    const double minlon = minlonValue ? args::get(minlonValue) : 0;
    const double maxlon = maxlonValue ? args::get(maxlonValue) : 0;
    const double minlat = minlatValue ? args::get(minlatValue) : 0;
    const double maxlat = maxlatValue ? args::get(maxlatValue) : 0;

    // Try to load the apikey from the environment.
    const char* apikeyEnv = getenv("MLN_API_KEY");
    const std::string apikey = apikeyValue ? args::get(apikeyValue) : (apikeyEnv ? apikeyEnv : std::string());

    const bool debug = debugFlag ? args::get(debugFlag) : false;

    using namespace mbgl;

    //auto mapTilerConfiguration = mbgl::TileServerOptions::MapTilerConfiguration();
    //std::string style = styleValue ? args::get(styleValue) : mapTilerConfiguration.defaultStyles().at(0).getUrl();
    std::string style = args::get(styleValue);

    util::RunLoop loop;

    HeadlessFrontend frontend({width, height}, static_cast<float>(pixelRatio));
    Map map(frontend,
        MapObserver::nullObserver(),
        MapOptions()
        .withMapMode(MapMode::Static)
        .withSize(frontend.getSize())
        .withPixelRatio(static_cast<float>(pixelRatio)),
        ResourceOptions()
        .withCachePath(cache_file)
        .withAssetPath(asset_root)
        .withApiKey(apikey));
                //.withTileServerOptions(mapTilerConfiguration));

    if (style.find("://") == std::string::npos) {
        style = std::string("file://") + style;
    }

    map.getStyle().loadURL(style);
    //map.jumpTo(CameraOptions().withCenter(LatLng{lat, lon}).withZoom(zoom).withBearing(bearing).withPitch(pitch));
    std::vector<LatLng> latLngs = { mbgl::LatLng{ minlat, minlon}, mbgl::LatLng{ maxlat, maxlon} };
    map.jumpTo(map.cameraForLatLngs(latLngs, {0, 0, 0, 0}));

    if (debug) {
        map.setDebug(debug ? mbgl::MapDebugOptions::TileBorders | mbgl::MapDebugOptions::ParseStatus
                           : mbgl::MapDebugOptions::NoDebug);
    }

    try {
        std::ofstream out(output, std::ios::binary);
        out << encodePNG(frontend.render(map).image);
        out.close();
    } catch (std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        exit(1);
    }

    CameraOptions CamOpts = map.getCameraOptions();
    LatLngBounds CamBounds = map.latLngBoundsForCamera(CamOpts);
    std::cout 
        << "Center (lon, lat): [" 
        << std::setprecision(std::numeric_limits<double>::digits10) << CamOpts.center->longitude() 
        << ", " 
        << std::setprecision(std::numeric_limits<double>::digits10) << CamOpts.center->latitude() 
        << "] Zoom: " 
        << std::setprecision(std::numeric_limits<double>::digits10) << *CamOpts.zoom 
        << std::endl;
    std::cout << "#"
        << std::setprecision(std::numeric_limits<double>::digits10) << *CamOpts.zoom
        << "/"
        << std::setprecision(std::numeric_limits<double>::digits10) << CamOpts.center->latitude()
        << "/"
        << std::setprecision(std::numeric_limits<double>::digits10) << CamOpts.center->longitude()
        << std::endl;

    std::cout << "west, east, south, north: "
        << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.west()
        << ","
        << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.east()
        << ","
        << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.south()
        << ","
        << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.north()
        << std::endl;
    std::ofstream myfile;
    myfile.open("bbox.csv");
    myfile << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.west() << "\n";
    myfile << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.east() << "\n";
    myfile << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.south() << "\n";
    myfile << std::setprecision(std::numeric_limits<double>::digits10) << CamBounds.north() << "\n";
    myfile.close();

    return 0;
}
