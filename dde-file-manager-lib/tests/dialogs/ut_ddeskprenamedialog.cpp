#include "dialogs/ddesktoprenamedialog.h"

#include <gtest/gtest.h>

namespace  {
    class TestDDesktopRenameDialog : public testing::Test
    {
    public:
        void SetUp() override
        {
            m_pTester = new DDesktopRenameDialog();
            std::cout << "start TestDDesktopRenameDialog";
        }
        void TearDown() override
        {
            std::cout << "end TestDDesktopRenameDialog";
        }
    public:
        DDesktopRenameDialog *m_pTester;
    };
}

TEST_F(TestDDesktopRenameDialog, testInit)
{
//    m_pTester->show();
}

TEST_F(TestDDesktopRenameDialog, testGetCurrentModeIndex)
{
    std::size_t result = m_pTester->getCurrentModeIndex();
    EXPECT_EQ(result, 0);
}

TEST_F(TestDDesktopRenameDialog, testGetAddMode)
{
    DFileService::AddTextFlags result = m_pTester->getAddMode();
    EXPECT_EQ(result, DFileService::AddTextFlags::Before);
}

TEST_F(TestDDesktopRenameDialog, testGetModeOneContent)
{
    QPair<QString, QString> result = m_pTester->getModeOneContent();
    EXPECT_STREQ(result.first.toStdString().c_str(), "");
    EXPECT_STREQ(result.second.toStdString().c_str(), "");
}

TEST_F(TestDDesktopRenameDialog, testGetModeTwoContent)
{
    QPair<QString, DFileService::AddTextFlags> result = m_pTester->getModeTwoContent();
    EXPECT_STREQ(result.first.toStdString().c_str(), "");
    EXPECT_EQ(result.second, DFileService::AddTextFlags::Before);
}

TEST_F(TestDDesktopRenameDialog, testGetModeThreeContent)
{
    QPair<QString, QString> result = m_pTester->getModeThreeContent();
    EXPECT_STREQ(result.first.toStdString().c_str(), "");
    EXPECT_STREQ(result.second.toStdString().c_str(), "1");
}

TEST_F(TestDDesktopRenameDialog, testSetVisible)
{
    m_pTester->setVisible(false);
}

TEST_F(TestDDesktopRenameDialog, testSetDialogTitle)
{
    m_pTester->setDialogTitle("UnitTestTitle");
}
