####################################################################################################################################
# Mock Archive Tests
####################################################################################################################################
package pgBackRestTest::Module::Mock::MockArchiveTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Archive::Info;
use pgBackRest::Backup::Info;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# archiveCheck
#
# Check that a WAL segment is present in the repository.
####################################################################################################################################
sub archiveCheck
{
    my $self = shift;
    my $strArchiveFile = shift;
    my $strArchiveChecksum = shift;
    my $bCompress = shift;
    my $strSpoolPath = shift;

    # Build the archive name to check for at the destination
    my $strArchiveCheck = PG_VERSION_94 . "-1/${strArchiveFile}-${strArchiveChecksum}";

    if ($bCompress)
    {
        $strArchiveCheck .= '.gz';
    }

    my $oWait = waitInit(5);
    my $bFound = false;

    do
    {
        $bFound = storageRepo()->exists(STORAGE_REPO_ARCHIVE . "/${strArchiveCheck}");
    }
    while (!$bFound && waitMore($oWait));

    if (!$bFound)
    {
        confess 'unable to find ' . storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/${strArchiveCheck}");
    }

    if (defined($strSpoolPath))
    {
        storageTest()->remove("${strSpoolPath}/archive/" . $self->stanza() . "/out/${strArchiveFile}.ok");
    }
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $strArchiveChecksum = '72b9da071c13957fb4ca31f05dbd5c644297c2f7';

    foreach my $bS3 (false, true)
    {
    foreach my $bRemote ($bS3 ? (true) : (false, true))
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin(
            "rmt ${bRemote}, s3 ${bS3}", $self->processMax() == 1)) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(),
            {bHostBackup => $bRemote, bCompress => false, bArchiveAsync => false, bS3 => $bS3});

        # Reduce console logging to detail
        $oHostDbMaster->configUpdate({&CONFIG_SECTION_GLOBAL => {&OPTION_LOG_LEVEL_CONSOLE => lc(DETAIL)}});
        my $strOptionLogDebug = '--' . OPTION_LOG_LEVEL_CONSOLE . '=' . lc(DEBUG);

        # Create the xlog path
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        storageTest()->pathCreate($strXlogPath, {bCreateParent => true});

        # Create the test path for pg_control
        storageTest()->pathCreate($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL, {bCreateParent => true});

        # Copy pg_control for stanza-create
        storageTest()->copy(
            $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin',
            $oHostDbMaster->dbBasePath() . qw{/} . DB_FILE_PGCONTROL);

        # Create archive-push command
        my $strCommand =
            $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() . ' --stanza=db archive-push';

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    archive.info missing');
        my $strSourceFile1 = $self->walSegment(1, 1, 1);
        storageTest()->pathCreate("${strXlogPath}/archive_status");
        my $strArchiveFile1 = $self->walGenerate($strXlogPath, WAL_VERSION_94, 1, $strSourceFile1);

        $oHostDbMaster->executeSimple($strCommand . " ${strXlogPath}/${strSourceFile1} --archive-max-mb=24",
            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate('stanza create', {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    push first WAL');

        my @stryExpectedWAL;
        my $strSourceFile = $self->walSegment(1, 1, 1);
        my $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 2, $strSourceFile);

        $oHostDbMaster->executeSimple(
            $strCommand . ($bRemote ? ' --cmd-ssh=/usr/bin/ssh' : '') . " ${strOptionLogDebug} ${strXlogPath}/${strSourceFile}",
            {oLogTest => $self->expect()});
        push(@stryExpectedWAL, "${strSourceFile}-${strArchiveChecksum}");

        # Test that the WAL was pushed
        $self->archiveCheck($strSourceFile, $strArchiveChecksum, false);

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    push second WAL');

        # Generate second WAL segment
        $strSourceFile = $self->walSegment(1, 1, 2);
        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 2, $strSourceFile);

        # Create a temp file to make sure it is deleted later (skip when S3 since it doesn't use temp files)
        my $strArchiveTmp;

        if (!$bS3)
        {
            # Should succeed when temp file already exists
            &log(INFO, '    succeed when tmp WAL file exists');

            $strArchiveTmp =
                $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
                    substr($strSourceFile, 0, 16) . "/${strSourceFile}-${strArchiveChecksum}." . COMPRESS_EXT . qw{.} .
                    STORAGE_TEMP_EXT;

            executeTest('sudo chmod 770 ' . dirname($strArchiveTmp));
            storageTest()->put($strArchiveTmp, 'JUNK');

            if ($bRemote)
            {
                executeTest('sudo chown ' . $oHostBackup->userGet() . " ${strArchiveTmp}");
            }
        }

        # Push the WAL
        $oHostDbMaster->executeSimple(
            "${strCommand} --compress --archive-async --process-max=2 ${strXlogPath}/${strSourceFile}",
            {oLogTest => $self->expect()});
        push(@stryExpectedWAL, "${strSourceFile}-${strArchiveChecksum}." . COMPRESS_EXT);

        # Make sure the temp file no longer exists if it was created
        if (defined($strArchiveTmp))
        {
            my $oWait = waitInit(5);
            my $bFound = true;

            do
            {
                $bFound = storageTest()->exists($strArchiveTmp);
            }
            while ($bFound && waitMore($oWait));

            if ($bFound)
            {
                confess "${strArchiveTmp} should have been removed by archive command";
            }
        }

        # Test that the WAL was pushed
        $self->archiveCheck($strSourceFile, $strArchiveChecksum, true, $oHostDbMaster->spoolPath());

        storageTest()->remove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    db version mismatch error');

        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
            {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}});

        $oHostDbMaster->executeSimple(
            $strCommand . " ${strXlogPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    db system-id mismatch error');

        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_BACKUP_KEY_SYSTEM_ID => 5000900090001855000}});

        $oHostDbMaster->executeSimple(
            $strCommand . " ${strXlogPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        # Restore the file to its original condition
        $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE));

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    stop');

        $oHostDbMaster->stop({strStanza => $oHostDbMaster->stanza()});

        $oHostDbMaster->executeSimple(
            $strCommand . " ${strXlogPath}/${strSourceFile}",
            {oLogTest => $self->expect(), iExpectedExitStatus => ERROR_STOP});

        $oHostDbMaster->start({strStanza => $oHostDbMaster->stanza()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    WAL duplicate ok');

        $oHostDbMaster->executeSimple($strCommand . " ${strXlogPath}/${strSourceFile}", {oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    WAL duplicate error');

        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 1, $strSourceFile);

        $oHostDbMaster->executeSimple(
            $strCommand . " ${strXlogPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL');

        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 2, "${strSourceFile}.partial");
        $oHostDbMaster->executeSimple(
            $strCommand . " ${strXlogPath}/${strSourceFile}.partial",
            {oLogTest => $self->expect()});
        $self->archiveCheck("${strSourceFile}.partial", $strArchiveChecksum, false);

        push(@stryExpectedWAL, "${strSourceFile}.partial-${strArchiveChecksum}");

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL duplicate');

        $oHostDbMaster->executeSimple(
            $strCommand . " ${strXlogPath}/${strSourceFile}.partial", {oLogTest => $self->expect()});
        $self->archiveCheck(
            "${strSourceFile}.partial", $strArchiveChecksum, false);

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL with different checksum');

        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 1, "${strSourceFile}.partial");
        $oHostDbMaster->executeSimple(
            $strCommand . " ${strXlogPath}/${strSourceFile}.partial",
            {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageRepo()->list(STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . '-1/0000000100000001')},
            '(' . join(', ', @stryExpectedWAL) . ')',
            'all WAL in archive', {iWaitSeconds => 5});
    }
    }
}

1;