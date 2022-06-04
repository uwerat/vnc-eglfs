#include <qpa/qplatformintegrationplugin.h>
#include <qpa/qplatformintegrationfactory_p.h>

#include <VncNamespace.h>

namespace
{
    class Plugin final : public QPlatformIntegrationPlugin
    {
        Q_OBJECT
        Q_PLUGIN_METADATA( IID QPlatformIntegrationFactoryInterface_iid FILE "metadata.json" )

      public:

        QPlatformIntegration* create( const QString& system,
            const QStringList& args, int& argc, char** argv ) override
        {
            QPlatformIntegration* integration = nullptr;

            if ( system.startsWith( "vnc", Qt::CaseInsensitive ) )
            {
                const auto path = QString::fromLocal8Bit(
                    qgetenv( "QT_QPA_PLATFORM_PLUGIN_PATH" ) );

                integration = QPlatformIntegrationFactory::create(
                    system.mid( 3 ), args, argc, argv, path );
            }

            if ( integration )
                Vnc::setAutoStartEnabled( true );

            return integration;
        }
    };
}

#include "VncProxyPlugin.moc"
