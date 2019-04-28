import * as React from "react";
import { createStyles, Theme, WithStyles, withStyles } from "@material-ui/core";
import Header from "./header";

const styles = ({ palette, spacing }: Theme) =>
  createStyles({
    root: {
      display: "flex",
      flexDirection: "column",
      padding: spacing.unit,
      backgroundColor: palette.background.default,
      color: palette.primary.main
    },
    main: {
      // width: '1100px',
      marginLeft: '1rem',
      marginRight: '1rem',      
    }
  });

interface IDefaultLayoutProps extends WithStyles<typeof styles> {
  children: React.ReactNode;
}

class DefaultLayout extends React.Component<IDefaultLayoutProps, any> {
  public render() {
    const { classes, children } = this.props;
    return (
      <div className={classes.root}>
        <Header />

        <main className={classes.main}>
          {children}
        </main>
      </div>
    );
  }
}

export default withStyles(styles)(DefaultLayout);
