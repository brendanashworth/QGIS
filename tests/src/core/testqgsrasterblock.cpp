/***************************************************************************
     testqgsrasterblock.cpp
     --------------------------------------
    Date                 : January 2017
    Copyright            : (C) 2017 by Martin Dobias
    Email                : wonder.sk at gmail.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"
#include <QObject>
#include <QString>
#include <QTemporaryFile>
#include <QLocale>

#include "qgsrasterlayer.h"
#include "qgsrasterdataprovider.h"

#include "qgsrasterprojector.h"
#include "qgscoordinatetransform.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsproject.h"
#include <QElapsedTimer>

/**
 * \ingroup UnitTests
 * This is a unit test for the QgsRasterBlock class.
 */
class TestQgsRasterBlock : public QObject
{
    Q_OBJECT
  public:
    TestQgsRasterBlock() = default;

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {} // will be called before each testfunction is executed.
    void cleanup() {} // will be called after every testfunction.

    void testBasic();
    void testProjected();
    void testWrite();
    void testPrintValueFloat_data();
    void testPrintValueFloat();
    void testPrintValueDouble_data();
    void testPrintValueDouble();

  private:

    QString mTestDataDir;
    QgsRasterLayer *mpRasterLayer = nullptr;
    QgsRasterLayer *demRasterLayer = nullptr;
};


//runs before all tests
void TestQgsRasterBlock::initTestCase()
{
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();

  mTestDataDir = QStringLiteral( TEST_DATA_DIR ); //defined in CmakeLists.txt
  const QString band1byteRaster = mTestDataDir + "/raster/band1_byte_ct_epsg4326.tif";

  mpRasterLayer = new QgsRasterLayer( band1byteRaster, QStringLiteral( "band1_byte" ) );

  QVERIFY( mpRasterLayer && mpRasterLayer->isValid() );

  demRasterLayer = new QgsRasterLayer( mTestDataDir + "/raster/dem.tif", "dem", "gdal" );
  QVERIFY( demRasterLayer && demRasterLayer->isValid() );
}

//runs after all tests
void TestQgsRasterBlock::cleanupTestCase()
{
  delete mpRasterLayer;

  QgsApplication::exitQgis();
}

void TestQgsRasterBlock::testBasic()
{
  QgsRasterDataProvider *provider = mpRasterLayer->dataProvider();
  QVERIFY( provider );

  const QgsRectangle fullExtent = mpRasterLayer->extent();
  const int width = mpRasterLayer->width();
  const int height = mpRasterLayer->height();

  QgsRasterBlock *block = provider->block( 1, fullExtent, width, height );

  bool isNoData = false;

  QCOMPARE( block->width(), 10 );
  QCOMPARE( block->height(), 10 );

  QVERIFY( block->isValid() );
  QVERIFY( !block->isEmpty() );
  QCOMPARE( block->dataType(), Qgis::DataType::Byte );
  QCOMPARE( block->dataTypeSize(), 1 );
  QVERIFY( block->hasNoDataValue() );
  QVERIFY( block->hasNoData() );
  QCOMPARE( block->noDataValue(), 255. );

  // value() with row, col
  QCOMPARE( block->value( 0, 0 ), 2. );
  QCOMPARE( block->value( 0, 1 ), 5. );
  QCOMPARE( block->value( 1, 0 ), 27. );
  QCOMPARE( block->valueAndNoData( 0, 0, isNoData ), 2. );
  QVERIFY( !isNoData );
  QCOMPARE( block->valueAndNoData( 0, 1, isNoData ), 5. );
  QVERIFY( !isNoData );
  QCOMPARE( block->valueAndNoData( 1, 0, isNoData ), 27. );
  QVERIFY( !isNoData );
  QVERIFY( std::isnan( block->valueAndNoData( mpRasterLayer->width() + 1, 0, isNoData ) ) );
  QVERIFY( isNoData );

  // value() with index
  QCOMPARE( block->value( 0 ), 2. );
  QCOMPARE( block->value( 1 ), 5. );
  QCOMPARE( block->value( 10 ), 27. );
  QCOMPARE( block->valueAndNoData( 0, isNoData ), 2. );
  QVERIFY( !isNoData );
  QCOMPARE( block->valueAndNoData( 1, isNoData ), 5. );
  QVERIFY( !isNoData );
  QCOMPARE( block->valueAndNoData( 10, isNoData ), 27. );
  QVERIFY( !isNoData );
  QVERIFY( std::isnan( block->valueAndNoData( mpRasterLayer->width() * mpRasterLayer->height(), isNoData ) ) );
  QVERIFY( isNoData );

  // isNoData()
  QCOMPARE( block->isNoData( 0, 1 ), false );
  QCOMPARE( block->isNoData( 0, 2 ), true );
  QCOMPARE( block->isNoData( 1 ), false );
  QCOMPARE( block->isNoData( 2 ), true );
  QCOMPARE( block->valueAndNoData( 0, 1, isNoData ), 5. );
  QVERIFY( !isNoData );
  block->valueAndNoData( 0, 2, isNoData );
  QVERIFY( isNoData );
  QCOMPARE( block->valueAndNoData( 1, isNoData ), 5. );
  QVERIFY( !isNoData );
  block->valueAndNoData( 2, isNoData );
  QVERIFY( isNoData );

  // data()
  const QByteArray data = block->data();
  QCOMPARE( data.count(), 100 );  // 10x10 raster with 1 byte/pixel
  QCOMPARE( data.at( 0 ), ( char ) 2 );
  QCOMPARE( data.at( 1 ), ( char ) 5 );
  QCOMPARE( data.at( 10 ), ( char ) 27 );

  // setData()
  const QByteArray newData( "\xaa\xbb\xcc\xdd" );
  block->setData( newData, 1 );
  const QByteArray data2 = block->data();
  QCOMPARE( data2.at( 0 ), ( char ) 2 );
  QCOMPARE( data2.at( 1 ), '\xaa' );
  QCOMPARE( data2.at( 2 ), '\xbb' );
  QCOMPARE( data2.at( 10 ), ( char ) 27 );

  delete block;
}

double averageValue(const QgsRasterBlock* block)
{
  if (!block || block->isEmpty() || block->dataType() != Qgis::DataType::Float32)
    return 0.0;

  double sum = 0.0;
  int width = block->width();
  int height = block->height();
  int count = 0;
  bool isNoData = false;

  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      double value = block->valueAndNoData(y * width + x, isNoData);
      if (!isNoData)
      {
        sum += value;
        ++count;
      }
    }
  }

  return count > 0 ? sum / count : 0.0;
}

uint64_t improvedHash(const QgsRasterBlock* block)
{
  if (!block || block->isEmpty() || block->dataType() != Qgis::DataType::Float32)
    return 0;

  uint64_t hash = 0;
  int width = block->width();
  int height = block->height();
  bool isNoData = false;

  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      double value = block->valueAndNoData(y * width + x, isNoData);
      if (isNoData)
        continue;

      uint64_t pixelHash = static_cast<uint64_t>(value * 1000);
      pixelHash ^= static_cast<uint64_t>(x) << 16;
      pixelHash ^= static_cast<uint64_t>(y) << 24;

      hash = ((hash << 5) + hash) + pixelHash;
    }
  }

  return hash;
}


void TestQgsRasterBlock::testProjected()
{
  // DEM raster layer is in 4326
  QgsRasterDataProvider *provider = demRasterLayer->dataProvider();
  QVERIFY( provider );

  const QgsRectangle extent_4326 = demRasterLayer->extent();
  const int width = demRasterLayer->width();
  const int height = demRasterLayer->height();
  qDebug() << "Width:" << width << "Height:" << height;

  // Create a projector to reproject to EPSG:3857
  QgsCoordinateReferenceSystem destCrs( QStringLiteral( "EPSG:3857" ) );
  QgsRasterProjector projector;
  projector.setInput( provider );
  projector.setCrs( provider->crs(), destCrs, QgsProject::instance()->transformContext() );

  // Project extent_4326 to 3857
  QgsCoordinateTransform transform( provider->crs(), destCrs, QgsProject::instance()->transformContext() );
  QgsRectangle extent_3857 = transform.transformBoundingBox( extent_4326 );

  qDebug() << "Original extent:" << extent_4326.toString();
  qDebug() << "Projected extent:" << extent_3857.toString();

  // Create extent_3857_lg which is extent_3857 except 50% larger in each direction
  QgsRectangle extent_3857_lg = extent_3857;
  double widthIncrease = extent_3857.width() * 0.5;
  double heightIncrease = extent_3857.height() * 0.5;
  extent_3857_lg.setXMinimum(extent_3857_lg.xMinimum() - widthIncrease / 2);
  extent_3857_lg.setXMaximum(extent_3857_lg.xMaximum() + widthIncrease / 2);
  extent_3857_lg.setYMinimum(extent_3857_lg.yMinimum() - heightIncrease / 2);
  extent_3857_lg.setYMaximum(extent_3857_lg.yMaximum() + heightIncrease / 2);

  qDebug() << "Larger extent:" << extent_3857_lg.toString();

  QgsRasterBlock *block1 = provider->block( 1, extent_4326, width, height );
  qDebug() << "Block 1 average value:" << averageValue(block1);
  qDebug() << "Block 1 hash:" << improvedHash(block1);
  QVERIFY((averageValue(block1) - 147.172) < 0.01);
  QCOMPARE(improvedHash(block1), 14202694558507027827ULL);

  QgsRasterBlock *block2 = projector.block( 1, extent_3857, width, height );
  qDebug() << "Block 2 average value:" << averageValue(block2);
  qDebug() << "Block 2 hash:" << improvedHash(block2);
  QVERIFY((averageValue(block2) - 147.159) < 0.01);
  QCOMPARE(improvedHash(block2), 9471715072450137233ULL);

  QgsRasterBlock *block3 = projector.block( 1, extent_3857_lg, width, height );
  qDebug() << "Block 3 average value:" << averageValue(block3);
  qDebug() << "Block 3 hash:" << improvedHash(block3);
  QVERIFY((averageValue(block3) - 147.183) < 0.01);
  QCOMPARE(improvedHash(block3), 13912239206301413414ULL);

  // Measure time to run block() with original extent
  QElapsedTimer timer;
  timer.start();

  // empty means that there's no data allocated, which is weird!

  const int N_RUNS = 50;
  for (int z_ = 0; z_ < N_RUNS; z_++) {
    // Time to run 10k block() with original extent: 10641 ms
    // so provider->block() is 1.06ms per call
    QgsRasterBlock *block = provider->block( 1, extent_4326, width, height );

    QVERIFY( block->isValid() );
    QVERIFY( !block->isEmpty() );

    delete block;
  }

  qDebug() << "Time to run" << N_RUNS << "block() with original extent:" << timer.elapsed() << "ms";
  QgsDebugError( QStringLiteral( "Time to run %1 block() with original extent: %2 ms" ).arg( N_RUNS ).arg( timer.elapsed() ) );

  // Measure time to run block() with projected extent
  timer.restart();

  // QgsRasterProjector::block is 75% of the compute here
  // and 51% is ProjectorData::srcRowCol, with 49% being ProjectorData::approximateSrcRowCol
  // 19% destPointOnCPMatrix

  for (int z_ = 0; z_ < N_RUNS; z_++) {
    // Time to run N_RUNS block() with projected extent: 7785 ms for 500 runs
    QgsRasterBlock *block = projector.block( 1, extent_3857, width, height );

    QVERIFY( block->isValid() );
    QVERIFY( !block->isEmpty() ); // HELP this is where it fails

    delete block;
  }

  qDebug() << "Time to run" << N_RUNS << "block() with projected extent:" << timer.elapsed() << "ms";
  QgsDebugError( QStringLiteral( "Time to run %1 block() with projected extent: %2 ms" ).arg( N_RUNS ).arg( timer.elapsed() ) );

  timer.restart();

  for (int z_ = 0; z_ < N_RUNS; z_++) {
    // Time to run N_RUNS block() with larger extent: 6134 ms for 500 runs
    QgsRasterBlock *block = projector.block( 1, extent_3857_lg, width, height );

    QVERIFY( block->isValid() );
    QVERIFY( !block->isEmpty() );

    delete block;
  }

  qDebug() << "Time to run" << N_RUNS << "block() with larger extent:" << timer.elapsed() << "ms";
  QgsDebugError( QStringLiteral( "Time to run %1 block() with larger extent: %2 ms" ).arg( N_RUNS ).arg( timer.elapsed() ) );



// QDEBUG : TestQgsRasterBlock::testProjected() Time to run N_RUNS block() with original extent: 22 ms
// tests/src/core/testqgsrasterblock.cpp:227 : (testProjected) [37ms] Time to run N_RUNS block() with original extent: 22 ms
// QDEBUG : TestQgsRasterBlock::testProjected() Time to run N_RUNS block() with projected extent: 554 ms
// tests/src/core/testqgsrasterblock.cpp:242 : (testProjected) [554ms] Time to run N_RUNS block() with projected extent: 554 ms
// QDEBUG : TestQgsRasterBlock::testProjected() Time to run N_RUNS block() with larger extent: 462 ms
// tests/src/core/testqgsrasterblock.cpp:256 : (testProjected) [462ms] Time to run N_RUNS block() with larger extent: 462 ms

  // Don't verify contents, just clean up
  // delete block;
}

void TestQgsRasterBlock::testWrite()
{
  const QgsRectangle extent = mpRasterLayer->extent();
  int nCols = mpRasterLayer->width(), nRows = mpRasterLayer->height();
  QVERIFY( nCols > 0 );
  QVERIFY( nRows > 0 );
  double tform[] =
  {
    extent.xMinimum(), extent.width() / nCols, 0.0,
    extent.yMaximum(), 0.0, -extent.height() / nRows
  };

  // generate unique filename (need to open the file first to generate it)
  QTemporaryFile tmpFile;
  tmpFile.open();
  tmpFile.close();

  // create a GeoTIFF - this will create data provider in editable mode
  const QString filename = tmpFile.fileName();
  QgsRasterDataProvider *dp = QgsRasterDataProvider::create( QStringLiteral( "gdal" ), filename, QStringLiteral( "GTiff" ), 1, Qgis::DataType::Byte, 10, 10, tform, mpRasterLayer->crs() );

  QgsRasterBlock *block = mpRasterLayer->dataProvider()->block( 1, mpRasterLayer->extent(), mpRasterLayer->width(), mpRasterLayer->height() );

  QByteArray origData = block->data();
  origData.detach();  // make sure we have private copy independent from independent block content
  QCOMPARE( origData.at( 0 ), ( char ) 2 );
  QCOMPARE( origData.at( 1 ), ( char ) 5 );

  // change first two pixels
  block->setData( QByteArray( "\xa0\xa1" ) );
  bool res = dp->writeBlock( block, 1 );
  QVERIFY( res );

  QgsRasterBlock *block2 = dp->block( 1, mpRasterLayer->extent(), mpRasterLayer->width(), mpRasterLayer->height() );
  const QByteArray newData2 = block2->data();
  QCOMPARE( newData2.at( 0 ), '\xa0' );
  QCOMPARE( newData2.at( 1 ), '\xa1' );

  delete block2;
  delete dp;

  // newly open raster and verify the write was permanent
  QgsRasterLayer *rlayer = new QgsRasterLayer( filename, QStringLiteral( "tmp" ), QStringLiteral( "gdal" ) );
  QVERIFY( rlayer->isValid() );
  QgsRasterBlock *block3 = rlayer->dataProvider()->block( 1, rlayer->extent(), rlayer->width(), rlayer->height() );
  const QByteArray newData3 = block3->data();
  QCOMPARE( newData3.at( 0 ), '\xa0' );
  QCOMPARE( newData3.at( 1 ), '\xa1' );

  QgsRasterBlock *block4 = new QgsRasterBlock( Qgis::DataType::Byte, 1, 2 );
  block4->setData( QByteArray( "\xb0\xb1" ) );

  // cannot write when provider is not editable
  res = rlayer->dataProvider()->writeBlock( block4, 1 );
  QVERIFY( !res );

  // some sanity checks
  QVERIFY( !rlayer->dataProvider()->isEditable() );
  res = rlayer->dataProvider()->setEditable( false );
  QVERIFY( !res );

  // make the provider editable
  res = rlayer->dataProvider()->setEditable( true );
  QVERIFY( res );
  QVERIFY( rlayer->dataProvider()->isEditable() );

  res = rlayer->dataProvider()->writeBlock( block4, 1 );
  QVERIFY( res );

  // finish the editing session
  res = rlayer->dataProvider()->setEditable( false );
  QVERIFY( res );
  QVERIFY( !rlayer->dataProvider()->isEditable() );

  // verify the change is there
  QgsRasterBlock *block5 = rlayer->dataProvider()->block( 1, rlayer->extent(), rlayer->width(), rlayer->height() );
  const QByteArray newData5 = block5->data();
  QCOMPARE( newData5.at( 0 ), '\xb0' );
  QCOMPARE( newData5.at( 1 ), '\xa1' ); // original data
  QCOMPARE( newData5.at( 10 ), '\xb1' );

  delete block3;
  delete block4;
  delete block5;
  delete rlayer;

  delete block;
}

void TestQgsRasterBlock::testPrintValueDouble_data( )
{
  QTest::addColumn< double  >( "value" );
  QTest::addColumn< bool  >( "localized" );
  QTest::addColumn< QLocale::Language >( "language" );
  QTest::addColumn< QString >( "expected" );

  QTest::newRow( "English double" ) << 123456.789 << true << QLocale::Language::English << QStringLiteral( "123,456.789" );
  QTest::newRow( "English int" ) << 123456.0 << true << QLocale::Language::English << QStringLiteral( "123,456" );
  QTest::newRow( "English int no locale" ) << 123456.0  << false << QLocale::Language::English << QStringLiteral( "123456" );
  QTest::newRow( "English double no locale" ) << 123456.789  << false << QLocale::Language::English << QStringLiteral( "123456.789" );
  QTest::newRow( "English negative double" ) << -123456.789 << true << QLocale::Language::English << QStringLiteral( "-123,456.789" );

  QTest::newRow( "Italian double" ) << 123456.789 << true << QLocale::Language::Italian << QStringLiteral( "123.456,789" );
  QTest::newRow( "Italian int" ) << 123456.0 << true << QLocale::Language::Italian << QStringLiteral( "123.456" );
  QTest::newRow( "Italian int no locale" ) << 123456.0 << false << QLocale::Language::Italian << QStringLiteral( "123456" );
  QTest::newRow( "Italian double no locale" ) << 123456.789 << false << QLocale::Language::Italian << QStringLiteral( "123456.789" );
  QTest::newRow( "Italian negative double" ) << -123456.789 << true << QLocale::Language::Italian << QStringLiteral( "-123.456,789" );
}


void TestQgsRasterBlock::testPrintValueDouble()
{
  QFETCH( double, value );
  QFETCH( bool, localized );
  QFETCH( QLocale::Language, language );
  QFETCH( QString, expected );

  QLocale::setDefault( language );
  QString actual = QgsRasterBlock::printValue( value, localized );
  QCOMPARE( actual, expected );
  QLocale::setDefault( QLocale::Language::English );
}

void TestQgsRasterBlock::testPrintValueFloat_data( )
{
  QTest::addColumn< float  >( "value" );
  QTest::addColumn< bool  >( "localized" );
  QTest::addColumn< QLocale::Language >( "language" );
  QTest::addColumn< QString >( "expected" );

  QTest::newRow( "English float" ) << 123456.789f << true << QLocale::Language::English << QStringLiteral( "123,456.79" );
  QTest::newRow( "English int" ) << 123456.f << true << QLocale::Language::English << QStringLiteral( "123,456" );
  QTest::newRow( "English int no locale" ) << 123456.f << false << QLocale::Language::English << QStringLiteral( "123456" );
  QTest::newRow( "English float no locale" ) << 123456.789f << false << QLocale::Language::English << QStringLiteral( "123456.79" );
  QTest::newRow( "English negative float" ) << -123456.789f << true << QLocale::Language::English << QStringLiteral( "-123,456.79" );

  QTest::newRow( "Italian float" ) << 123456.789f << true << QLocale::Language::Italian << QStringLiteral( "123.456,79" );
  QTest::newRow( "Italian int" ) << 123456.f << true << QLocale::Language::Italian << QStringLiteral( "123.456" );
  QTest::newRow( "Italian int no locale" ) << 123456.f << false << QLocale::Language::Italian << QStringLiteral( "123456" );
  QTest::newRow( "Italian float no locale" ) << 123456.789f << false << QLocale::Language::Italian << QStringLiteral( "123456.79" );
  QTest::newRow( "Italian negative float" ) << -123456.789f << true << QLocale::Language::Italian << QStringLiteral( "-123.456,79" );
}

void TestQgsRasterBlock::testPrintValueFloat()
{
  QFETCH( float, value );
  QFETCH( bool, localized );
  QFETCH( QLocale::Language, language );
  QFETCH( QString, expected );

  QLocale::setDefault( language );
  QString actual = QgsRasterBlock::printValue( value, localized );
  QCOMPARE( actual, expected );
  QLocale::setDefault( QLocale::Language::English );
}

QGSTEST_MAIN( TestQgsRasterBlock )

#include "testqgsrasterblock.moc"
