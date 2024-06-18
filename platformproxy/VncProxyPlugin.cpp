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
            const QStringList& args ) override
        {
            // not used by QPlatformIntegrationFactory but who knows ...

            int argc = 0;
            char* argv = nullptr;

            return createIntegration( system, args, argc, &argv );
        }

        QPlatformIntegration* create( const QString& system,
            const QStringList& args, int& argc, char** argv ) override
        {
            return createIntegration( system, args, argc, argv );
        }

      private:
        QPlatformIntegration* createIntegration( const QString& system,
            const QStringList& args, int& argc, char** argv )
        {
            QPlatformIntegration* integration = nullptr;

            const auto key = effectiveKey( system );
            if ( !key.isEmpty() )
            {
                const auto path = QString::fromLocal8Bit(
                    qgetenv( "QT_QPA_PLATFORM_PLUGIN_PATH" ) );

                integration = QPlatformIntegrationFactory::create(
                    key, args, argc, argv, path );

                if ( integration )
                    Vnc::setAutoStartEnabled( true );
            }

            return integration;
        }

        QString effectiveKey( const QString& system ) const
        {
            if ( system.startsWith( QStringLiteral("vnc"), Qt::CaseInsensitive ) )
                return system.mid( 3 );

            return QString();
        }
    };
}

#include "VncProxyPlugin.moc"
